#ifndef MEM_SLAB_HPP
#    include "slab.hpp"
#endif

template<size_t N>
template<size_t X, typename>
Slab<N, true>::Slab(Bin<X>* outer): CHUNK(Bin<X>::CHUNK), UNIT(block(BLOCK, CHUNK)) {
    OUTER = outer;
    std::memcpy(&binder, &Bin<X>::VTABLE);
}

template<size_t N> Slab<N, true>::~Slab() {
    purge();
}

template<size_t N> void* Slab<N, true>::acquire() noexcept {
    if constexpr(N == 0) {
        return nullptr;
    }

    // check block
    if(!current) {
        current = fulls.pop(); // first: recycle
        if(!current) {
            current = partials.pop(); // second: recycle
            if(!current) {
                current = map(); // last: alloc
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
        fulls.push(current);
        current = nullptr; // prepare next chunk
    }
    --counter; // count

    return out;
}

template<size_t N>
void Slab<N, true>::release(void* in) noexcept {
    if constexpr(N == 0) return;

    static constexpr size_t MASK = CHUNK - 1; // e.g. if CHUNK 65536 then operate by 0xFFFF

    // get chunk info
    Chunk*    chunk = reinterpret_cast<Chunk*>(uintptr_t(in) & ~MASK);  // known UB but safe in practice
    ptrdiff_t index = ((uintptr_t(in) - Chunk::OFFSET) & MASK) / BLOCK; // optimize by compiler

    // check pool
    if(chunk->meta.outer != OUTER) std::abort();

    // set state and check
    chunk->state.off(index);
    if(chunk != current) {
        // usage empty -> partial
        if(chunk->meta.used == Chunk::COUNT) {
            fulls.remove(chunk);
            partials.push(chunk);
        }
        // usage partial -> full
        if(chunk->meta.used == 1) {
            partials.remove(chunk);
            if(OUTER != this) {
                binder.release(OUTER, chunk); // return to outer
            }
            else frees.push(chunk); // caching
        }
    }
    --chunk->meta.used; // decount
    ++counter;
}

template<size_t N> size_t Slab<N, true>::reserve(size_t cnt) {
    if(cnt == 0) return 0;       // no reserve
    if(counter >= cnt) return 0; // reserved

    cnt = (cnt - counter); // need count

    size_t generated = 0;
    for(; generated < cnt; generated += Chunk::COUNT) {
        Chunk* chunk = map();
        if(!chunk) {
            break; // failed
        }
        fulls.push(chunk); // insert
    }
    return generated; // create count
}

template<size_t N> size_t Slab<N, true>::trim(size_t in) {
    size_t cnt = 0;
    Chunk* del = fulls.pop(); // pop

    while(del != nullptr) {
        Chunk* temp = fulls.pop(); // pop
        unmap(del);                // delete
        del = temp;                // set next
        ++cnt;
    }

    return cnt;
}

template<size_t N> void Slab<N, true>::reclaim() {

}

template<size_t N> void Slab<N, true>::purge() {
    Cache* list[] = { &frees, &fulls, &partials };
    for(int i = 0, len = sizeof(list) / sizeof(*list); i < len; ++i) {
        Cache* stack = list[i];

        Header* curr = stack->pop(); // pop curr
        while(curr != nullptr) {
            Header* next = stack->pop(); // pop next
            unmap(curr);                 // delete curr
            curr = next;                 // curr to next
        }
    }
    if(current) {
        unmap(current);
    }
}

template<size_t N> size_t Slab<N, true>::usable() const {
    return counter;
}

template<size_t N> void* Slab<N, true>::map() noexcept {
    if(OUTER != this) {
        return binder.acquire(); // get from outer
    }

    Chunk* ptr;

    ptr = global::pal_valloc<Chunk>(CHUNK, CHUNK); // othesr: aligned to CHUNK
    if(ptr) {
        new(ptr) Chunk();                          // for life cycle
        static_cast<Header>(ptr)->outher  = OUTER; // set outer
        counter                          += UNIT;  // count
    }

    return ptr;
}

template<size_t N> void Slab<N, true>::unmap(void* in) noexcept {
    global::pal_vfree(in, CHUNK); // other: aligned to CHUNK
    counter -= UNIT;
}
