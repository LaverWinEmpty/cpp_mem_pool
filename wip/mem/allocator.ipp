#ifndef MEM_Allocator_HPP
#    include "allocator.hpp"
#endif

template<size_t N, typename T> struct Allocator<N, T>::Meta {
    size_t     used  = 0;
    Allocator* outer = 0;
    Chunk*     next  = nullptr;
    Chunk*     prev  = nullptr;
};

template<size_t N, typename T> struct Allocator<N, T>::Chunk {
    //! @brief the block total bits / data + flag bits
    static constexpr size_t COUNT = WHOLE ? 1 : (CHUNK - sizeof(Meta)) * 8 / (BLOCK * 8 + 1);

    //! @brief object count to byte, divied to sizeof(uint_64), and round up
    using State = core::Mask<(COUNT + 63) / 64>;

    //! @brief [ meta | state | PADDING | data ]
    static constexpr size_t OFFSET  = ((sizeof(Meta) + sizeof(State)) / BLOCK) * BLOCK; // align
    static constexpr size_t PADDING = OFFSET - (sizeof(Meta) + sizeof(State));

    //! @brief size check
    static_assert((sizeof(Meta) + sizeof(State) + PADDING + BLOCK * COUNT) <= CHUNK);

    Meta    meta;
    State   state;
    uint8_t data[CHUNK - sizeof(meta) - sizeof(state)];
};

template<size_t N, typename T> Allocator<N, T>::~Allocator() {
    Stack* list[3] = { &empty, &full, &partial };
    for(int i = 0; i < 3; ++i) {
        Stack* stack = list[i];

        Chunk* curr = stack->pop(); // pop curr
        while(curr != nullptr) {
            Chunk* next = stack->pop(); // pop next
            destroy(curr);              // delete curr
            curr = next;                // curr to next
        }
    }
    if(current) {
        destroy(current);
    }
}

template<size_t N, typename T>
template<typename U, typename... Args> U* Allocator<N, T>::acquire(Args&&... in) noexcept {
    if constexpr(N == 0) {
        return nullptr;
    }

    // huge pages
    if constexpr (WHOLE) {
        Chunk* temp = full.pop(); // pop
        if (!temp) {
            temp = generate(); // alloc
        }
        empty.push(temp); // push
        if constexpr(std::is_same_v<T, void>) {
            return temp; // return
        }
        else return CXX_LAUNDER(reinterpret_cast<U*>(temp)); // return with launder
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
    if constexpr(std::is_same_v<T, T> == false) {
        if constexpr(sizeof...(Args) != 0) {
            return new(out) T(std::forward<Args>(in)...);
        }
        else return new(out) T();
    }
    return static_cast<U*>(out);
}

template<size_t N, typename T>
template<typename U> void Allocator<N, T>::release(U* in) {
    // call destrcutor
    if constexpr(std::is_same_v<T, T> == false) {
        in->~T();
    }

    if constexpr(N == 0) return;

    // huge pages
    if constexpr (WHOLE) {
        Chunk* chunk = reinterpret_cast<Chunk*>(in);
        // check
        if(empty.remove(chunk) == false) {
            std::abort(); // not found
        }
        full.push(chunk); // OK
        return;
    }

    // get chunk info
    Chunk*    chunk;
    ptrdiff_t index;

    static constexpr size_t MASK = CHUNK - 1; // e.g. if CHUNK 65536 then operate by 0xFFFF

    // find chunk begin address
    chunk = reinterpret_cast<Chunk*>(uintptr_t(in) & ~MASK); // known UB but safe in practice

    // calculate index of the block within the chunk
    index = ((uintptr_t(in) - Chunk::OFFSET) & MASK) / BLOCK; // optimize by compiler

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

template<size_t N, typename T>
size_t Allocator<N, T>::reserve(size_t cnt) {
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

template<size_t N, typename T>
size_t Allocator<N, T>::shrink() {
    size_t cnt = 0;
    Chunk* del = full.pop(); // pop

    while(del != nullptr) {
        Chunk* temp = full.pop(); // pop
        destroy(del);             // delete
        del = temp;               // set next
        ++cnt;
    }

    // all clear
    if constexpr(WHOLE) {
        if(current) {
            destroy(current);
            current = nullptr;
            ++cnt;
        }
    }

    return cnt;
}

template<size_t N, typename T> auto Allocator<N, T>::generate() noexcept -> Chunk* {
    Chunk* ptr;

    // WHOLE CHUNK does not require align
    if constexpr(WHOLE) {
        ptr = global::pal_valloc<Chunk>(BLOCK); // 1 chunk == 1 block
    }
    
    else {
        ptr = global::pal_valloc<Chunk>(CHUNK, CHUNK); // other: aligned to CHUNK
        if(ptr) {
            new(ptr) Chunk;         // init for life cycle
            ptr->meta.outer = this; // set outer
        }
    }

    if(ptr) {
        counter += Chunk::COUNT; // add
    }
    return ptr;
}

template<size_t N, typename T> void Allocator<N, T>::destroy(Chunk* in) noexcept {

    // matches the parameter when pal_valloc is called
    if constexpr(WHOLE) {
        global::pal_vfree(in, BLOCK); // 1 chunk == 1 block
    }
    else {
        in->~Chunk();
        global::pal_vfree(in, CHUNK); // other: aligned to CHUNK
    }
    counter -= Chunk::COUNT;
}

template<size_t N, typename T> struct Allocator<N, T>::List {
    bool remove(Chunk* in) {
        Chunk* prev = in->meta.prev;
        Chunk* next = in->meta.next;
        if (prev) prev->meta.next = next;
        if (next) next->meta.prev = prev;
        if (in == head) head = next;

        return true;
    }

    bool push(Chunk* in) {
        in->meta.prev = nullptr;
        in->meta.next = head; // push front
        if (head) {
            head->meta.prev = in; // link
        }
        head = in; // new head

        return true;
    }

    Chunk* pop() {
        Chunk* out = head;
        if (out) {
            head = out->meta.next;
            out->meta.next = nullptr;
            out->meta.prev = nullptr;
        }
        return out;
    }

    Chunk* head = nullptr;
};

template<size_t N, typename T> struct Allocator<N, T>::Array {
    bool remove(Chunk* in) {
        for (int i = 0; i < top; ++i) {
            if (vec[i] == in) {
                --top;             // reduce
                vec[i] = vec[top]; // swap and delete
                return true;
            }
        }
        return false;
    }

    bool push(Chunk* in) {
        static constexpr size_t EX = (global::PAL_PAGE) / sizeof(void*);

        size_t old = cap * sizeof(void*);
        if(top >= cap) {
            Chunk** temp = global::pal_valloc<Chunk*>(old + global::PAL_PAGE); // alloc
            if (!temp) {
                return false; // failed
            }

            // realloc
            if(vec) {
                std::memcpy(temp, vec, old); // copy
                global::pal_vfree(vec, old); // free
            }

            // new vector
            vec = temp;
            cap += EX;
        }
        vec[top++] = in; // push
        return true;
    }

    Chunk* pop() {
        if (top == 0) {
            return nullptr;
        }
        return vec[--top];
    }

    Chunk** vec = nullptr;
    size_t  top = 0;
    size_t  cap = 0;
};
