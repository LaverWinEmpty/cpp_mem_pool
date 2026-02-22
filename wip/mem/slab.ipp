#ifndef MEM_SLAB_HPP
#    include "slab.hpp"
#endif

template<size_t N> struct Slab<N>::Meta {
    size_t used  = 0;
    Slab*  outer = 0;
    Chunk* next  = nullptr;
    Chunk* prev  = nullptr;
};

template<size_t N> struct Slab<N>::Primary {
    //! @tparam chunk size byte
    //! @brief the block total bits / data + flag bits
    static constexpr size_t COUNT = (CHUNK - sizeof(Meta)) * 8 / (BLOCK * 8 + 1);

    //! @brief object count to byte, divied to sizeof(uint_64), and round up
    using State = core::Mask<(COUNT + 63) / 64>;

    //! @brief [ meta | state | PADDING | data ]
    static constexpr size_t OFFSET  = ((sizeof(Meta) + sizeof(State)) / BLOCK) * BLOCK; // align
    static constexpr size_t PADDING = OFFSET - (sizeof(Meta) + sizeof(State));   

    // size check
    static_assert((sizeof(Meta) + sizeof(State) + PADDING + BLOCK * COUNT) <= CHUNK);

    Meta    meta;
    State   state;
    uint8_t data[CHUNK - sizeof(meta) - sizeof(state)];
};

template<size_t N> struct Slab<N>::Fallback {
    static constexpr size_t COUNT  = 1;
    static constexpr size_t OFFSET = 0;

    using State = core::Mask<1>; // unused

    uint8_t data[CHUNK];
    Meta    meta;
    State   state; // unused
};

// template<size_t N> Slab<N>::Slab() { }

template<size_t N> Slab<N>::~Slab() {
    Depot* list[3] = { &empty, &full, &partial };
    for(int i = 0; i < 3; ++i) {
        Depot* depot = list[i];

        Chunk* curr = depot->pop(); // pop curr
        while(curr != nullptr) {
            Chunk* next = depot->pop(); // pop next
            destroy(curr);              // delete curr
            curr = next;                // curr to next
        }
    }
    if(current) {
        destroy(current);
    }
}

template<size_t N>
template<typename T, typename... Args> T* Slab<N>::acquire(Args&&... in) noexcept {
    if constexpr(N == 0) {
        return nullptr;
    }

    // check block
    if(!current) {
        current = full.pop(); // first: recycle
        if(!current) {
            current = partial.pop(); // second: recycle
            if(!current) {
                current = generate(); // last: alloc
                if(!current) {
                    return nullptr; // failed
                }
            }
        }
    }

    // check state
    size_t index = current->state.next();
    current->state.on(index);

    // return
    void* out = reinterpret_cast<uint8_t*>(current) + Chunk::OFFSET + index * BLOCK;

    // get meta, and MAX to index
    // usage partial -> empty
    if(++current->meta.used > Chunk::COUNT - 1) {
        empty.push(current);
        current = nullptr; // prepare next chunk
    }
    --counter; // count

    // call constructor
    if constexpr(std::is_same_v<T, void> == false) {
        if constexpr(sizeof...(Args) != 0) {
            return new(out) T(std::forward<Args>(in)...);
        }
        else return new(out) T();
    }
    return out;
}

template<size_t N>
template<typename T> void Slab<N>::release(T* in) {
    // call destrcutor
    if constexpr(std::is_same_v<T, void> == false) {
        in->~T();
    }

    if constexpr(N == 0) return;

    // get chunk info
    Chunk*    chunk;
    ptrdiff_t index;

    if constexpr(HUGE) {
        chunk = reinterpret_cast<Chunk*>(in); // known UB but safe in practice
        index = 0;                            // fallback: chunk == block
    }
    else {
        static constexpr size_t MASK = CHUNK - 1; // if CHUNK 65536 then operate by 0xFFFF

        // find chunk begin address
        chunk = reinterpret_cast<Chunk*>(uintptr_t(in) & ~MASK); // known UB but safe in practice

        // calculate index of the block within the chunk
        index = ((uintptr_t(in) - Chunk::OFFSET) & MASK) / BLOCK; // optimize by compiler
    }

    // check pool
    if(chunk->meta.outer != this) std::abort();

    // set state and check
    chunk->state.off(index);
    if(chunk != current) {
        // usage empty -> partial
        if(chunk->meta.used == Chunk::COUNT) {
            empty.remove(chunk);
            partial.push(chunk);
        }
        // usage partial -> full
        if(chunk->meta.used == 1) {
            partial.remove(chunk);
            full.push(chunk);
        }
    }
    --chunk->meta.used; // decount
    ++counter;
}

template<size_t N>
size_t Slab<N>::reserve(size_t cnt) {
    if(cnt == 0) return 0;       // no reserve
    if(counter >= cnt) return 0; // reserved

    cnt = (cnt - counter); // need count

    size_t generated = 0;
    for(; generated < cnt; generated += Chunk::COUNT) {
        Chunk* chunk = generate();
        if(!chunk) {
            break; // failed
        }
        full.push(chunk); // insert
    }
    return generated; // create count
}

template<size_t N>
size_t Slab<N>::shrink() {
    size_t cnt = 0;
    Chunk* del = full.pop(); // pop

    while(del != nullptr) {
        Chunk* temp = full.pop(); // pop
        destroy(del);             // delete
        del = temp;               // set next
        ++cnt;
    }
    return cnt;
}

template<size_t N> auto Slab<N>::generate() noexcept -> Chunk* {
    Chunk* ptr;

    // HUGE CHUNK does not require align
    if constexpr(HUGE) {
        ptr = (Chunk*)global::pal_valloc(BLOCK + PAGE, PAGE); // HUGE: aligned to 4KiB
    }
    else ptr = (Chunk*)global::pal_valloc(CHUNK, CHUNK); // other: aligned to CHUNK

    if(ptr) {
        new(ptr) Chunk;

        ptr->meta.outer = this; // init

        counter += Chunk::COUNT; // add
    }

    return ptr;
}

template<size_t N> void Slab<N>::destroy(Chunk* in) noexcept {
    in->~Chunk();

    // matches the parameter when pal_valloc is called
    if constexpr(HUGE) {
        global::pal_vfree(in, BLOCK + PAGE, PAGE); // HUGE: aligned to 4KiB
    }
    else global::pal_vfree(in, CHUNK, CHUNK); // other: aligned to CHUNK

    counter -= Chunk::COUNT;
}

template<size_t N> void Slab<N>::Depot::remove(Chunk* in) {
    Chunk* prev = in->meta.prev;
    Chunk* next = in->meta.next;
    if(prev) prev->meta.next = next;
    if(next) next->meta.prev = prev;
    if(in == head) head = next;
}

template<size_t N> void Slab<N>::Depot::push(Chunk* in) {
    in->meta.prev = nullptr;
    in->meta.next = head; // push front
    if(head) {
        head->meta.prev = in; // link
    }
    head = in; // new head
}

template<size_t N> auto Slab<N>::Depot::pop() -> Chunk* {
    Chunk* out = head;
    if(out) {
        head           = out->meta.next;
        out->meta.next = nullptr;
        out->meta.prev = nullptr;
    }
    return out;
}
