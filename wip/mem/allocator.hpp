#ifndef MEM_ALLOCATOR
#define MEM_ALLOCATOR

#include "../core/mask.hpp"
#include "../global/pal.hpp"
#include <cassert>
#include <cstdlib>
#include <utility>
#include <type_traits>

//! @brief allocator instance
template<size_t N> class Allocator {
private:
    static constexpr size_t PAGE = 4096;
    static constexpr bool   HUGE = N >= (1 << 20); // 1MiB

private:
    struct Primary;
    struct Fallback;

private:
    //! @brief chunk size preset
    using Chunk = std::conditional_t<HUGE, Fallback, Primary>;

private:
    //! @brief footer
    struct Meta;

public:
    static constexpr size_t CHUNK =
        HUGE ? N + PAGE :                            // HUGE:   fallback: 1 chunk as 1 block, with meta
        (global::bit_pow2(N * 15) <= 65536 ? 65536 : // SMALL:  fixed 64KiB, default
            (global::bit_pow2(N * 15))               // MEDIUM: at least 15 guaranteed, for 4KiB based on 64KiB
        );
    static constexpr size_t UNIT  = Chunk::COUNT;

public:
    /**
     * @brief constructor
     */
    Allocator() = default;

public:
    /**
     * @brief destructor
     * @throw when chunk in use on debug
     */
    ~Allocator();

public:
    /**
     * @brief  malloc with placement new
     *
     * @tparam T type of the returned pointer
     * @param [in] args constructor parameters
     * @return nullptr if failed
     */
    template<typename T = void, typename... Args> T* acquire(Args&&... args) noexcept;

public:
    /**
     * @brief free, crash when called from a different pool
     *
     * @param [in] ptr pointer from valloc
     */
    template<typename T = void> void release(T*);

public:
    /**
     * @brief syscall: create chunks
     *
     * @param [in] block count
     * @return created chunks count
     */
    size_t reserve(size_t);

public:
    /**
     * @brief syscall: destroy empty chunks
     *
     * @return destroyed chunks count
     */
    size_t shrink();

public:
    /**
     * @brief change all chunks to empty state.
     * @throw when chunk in use on debug
     */
    void reset();

public:
    size_t usable() { return counter; }


private:
    //! @brief chunk single linked list
    struct Depot {
        void   remove(Chunk*);
        void   push(Chunk*);
        Chunk* pop();

        Chunk* head = nullptr;
    };

private:
    Depot full;    //!< chunks using block is 0
    Depot empty;   //!< chunks using block is full
    Depot partial; //!< chunks using block is ?

private:
    Chunk* current;     //!< using chunk
    size_t counter = 0; //!< usable block counter

private:
    //! @brief syscall allocate
    Chunk* generate() noexcept;

private:
    //! @brief syscall deallocate
    void destroy(Chunk*) noexcept;
};

template<size_t N> struct Allocator<N>::Meta {
    size_t     used  = 0;
    Allocator* outer = 0;
    Chunk*     next  = nullptr;
    Chunk*     prev  = nullptr;
};

template<size_t N> struct Allocator<N>::Primary {
    //! @tparam chunk size byte
    //! @brief the block total bits / data + flag bits
    static constexpr size_t COUNT = (CHUNK - sizeof(Meta)) * 8 / (N * 8 + 1);

    //! @brief object count to byte, divied to sizeof(uint_64), and round up
    using State = core::Mask<(COUNT + 63) / 64>;

    //! @brief [ meta | state | PADDING | data ]
    static constexpr size_t OFFSET  = global::bit_align(sizeof(Meta) + sizeof(State), N);
    static constexpr size_t PADDING = OFFSET - (sizeof(Meta) + sizeof(State));

    // size check
    static_assert((sizeof(Meta) + sizeof(State) + PADDING + N * COUNT) <= CHUNK);

    Meta    meta;
    State   state;
    uint8_t data[CHUNK - sizeof(meta) - sizeof(state)];
};

template<size_t N> struct Allocator<N>::Fallback {
    static constexpr size_t COUNT = 1;

    using State = core::Mask<1>; // unused

    uint8_t data[N];
    Meta    meta;
    State   state; // unused
};

// template<size_t N> Allocator<N>::Allocator() { }

template<size_t N> Allocator<N>::~Allocator() {
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
template<typename T, typename... Args> T* Allocator<N>::acquire(Args&&... in) noexcept {
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
    void* out = reinterpret_cast<uint8_t*>(current) + Chunk::OFFSET + index * N;

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
template<typename T> void Allocator<N>::release(T* in) {
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
        index = ((uintptr_t(in) - Chunk::OFFSET) & MASK) / N; // optimize by compiler
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
size_t Allocator<N>::reserve(size_t cnt) {
    if(cnt == 0) return 0;         // no reserve
    if(counter >= cnt) return 0;   // reserved

    cnt = (cnt - counter); // need count

    size_t generated = 0;
    for (; generated < cnt; generated += Chunk::COUNT) {
        Chunk* chunk = generate();
        if(!chunk) {
            break; // failed
        }
        full.push(chunk); // insert
    }
    return generated; // create count
}

template<size_t N>
size_t Allocator<N>::shrink() {
    size_t cnt = 0;
    Chunk* del = full.pop(); // pop

    while(del != nullptr) {
        Chunk* temp = full.pop(); // pop
        destroy(del);              // delete
        del = temp;                // set next
        ++cnt;
    }
    return cnt;
}

template<size_t N> auto Allocator<N>::generate() noexcept -> Chunk* {
    Chunk* ptr;

    // HUGE CHUNK does not require align
    if constexpr (HUGE) {
        ptr = (Chunk*)global::pal_valloc(N + PAGE, PAGE); // HUGE: aligned to 4KiB
    }
    else ptr = (Chunk*)global::pal_valloc(CHUNK, CHUNK); // other: aligned to CHUNK

    if(ptr) {
        new(ptr) Chunk;

        ptr->meta.outer = this; // init

        counter += Chunk::COUNT; // add
    }

    return ptr;
}

template<size_t N> void Allocator<N>::destroy(Chunk* in) noexcept {
    in->~Chunk();

    // matches the parameter when pal_valloc is called
    if constexpr (HUGE) {
        global::pal_vfree(in, N + PAGE, PAGE); // HUGE: aligned to 4KiB
    }
    else global::pal_vfree(in, CHUNK, CHUNK); // other: aligned to CHUNK

    counter -= Chunk::COUNT;
}

template<size_t N> void Allocator<N>::Depot::remove(Chunk* in) {
    Chunk* prev = in->meta.prev;
    Chunk* next = in->meta.next;
    if(prev) prev->meta.next = next;
    if(next) next->meta.prev = prev;
    if(in == head) head = next;
}

template<size_t N> void Allocator<N>::Depot::push(Chunk* in) {
    in->meta.prev = nullptr;
    in->meta.next = head; // push front
    if(head) {
        head->meta.prev = in; // link
    }
    head = in; // new head
}

template<size_t N> auto Allocator<N>::Depot::pop() -> Chunk* {
    Chunk* out = head;
    if(out) {
        head           = out->meta.next;
        out->meta.next = nullptr;
        out->meta.prev = nullptr;
    }
    return out;
}

#endif
