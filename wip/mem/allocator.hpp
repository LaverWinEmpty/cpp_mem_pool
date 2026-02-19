#ifndef MEM_ALLOCATOR
#define MEM_ALLOCATOR

#include "../core/mask.hpp"
#include "../global/pal.hpp"
#include <cassert>
#include <cstdlib>
#include <utility>

//! @brief allocator instance
template<size_t> class Allocator {
private:
    //! @brief chunk size preset
    struct Chunk;

private:
    //! @brief footer
    struct Meta;

public:
    static constexpr size_t CHUNK = 64 * 1024; // byte == 64KiB
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
     * @brief syscall: destroy full chunks
     *
     * @return destroyed chunks count
     */
    size_t shrink();

public:
    /**
     * @brief change all chunks to full state.
     * @throw when chunk in use on debug
     */
    void reset();

public:
    size_t remainder() { return usable; }


private:
    //! @brief chunk single linked list
    struct Depot {
        void   remove(Chunk*);
        void   push(Chunk*);
        Chunk* pop();

        Chunk* head = nullptr;
    };

private:
    Depot empty;   //!< chunks using block is 0
    Depot full;    //!< chunks using block is full
    Depot partial; //!< chunks using block is ?

private:
    Chunk* current;    //!< using chunk
    size_t usable = 0; //!< usable block count

private:
    //! @brief syscall allocate (64KiB aligned chunk)
    Chunk* generate() noexcept;

private:
    //! @brief syscall deallocate (64KiB aligned chunk)
    void destroy(Chunk*) noexcept;
};

template<size_t N> struct Allocator<N>::Meta {
    size_t     used  = 0;
    Allocator* outer = 0;
    Chunk*     next  = nullptr;
    Chunk*     prev  = nullptr;
};

template<size_t N> struct Allocator<N>::Chunk {
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

// template<size_t N> Allocator<N>::Allocator() { }

template<size_t N> Allocator<N>::~Allocator() {
    Depot* list[3] = { &full, &empty, &partial };
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
    if constexpr(Chunk::COUNT < 8) {
        return malloc(N);
    }

    // check block
    if(!current) {
        current = empty.pop(); // first: recycle
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
    // usage partial -> full
    if(++current->meta.used > Chunk::COUNT - 1) {
        full.push(current);
        current = nullptr; // prepare next chunk
    }
    --usable; // count

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
    if constexpr(Chunk::COUNT < 8) {
        free(in);
    }

    // get chunk info
    Chunk*    chunk = reinterpret_cast<Chunk*>(uintptr_t(in) & ~0xFFFF); // known UB but safe in practice
    ptrdiff_t index = ((uintptr_t(in) - Chunk::OFFSET) & 0xFFFF) / N;

    // check pool
    if(chunk->meta.outer != this) std::abort();

    // set state and check
    chunk->state.off(index);
    if(chunk != current) {
        // usage full -> partial
        if(chunk->meta.used == Chunk::COUNT) {
            full.remove(chunk);
            partial.push(chunk);
        }
        // usage partial -> empty
        if(chunk->meta.used == 1) {
            partial.remove(chunk);
            empty.push(chunk);
        }
    }
    --chunk->meta.used; // decount
    ++usable;
}

template<size_t N>
size_t Allocator<N>::reserve(size_t cnt) {
    if(cnt == 0) return 0;         // no reserve
    if(usable >= cnt) return 0;    // reserved
    if(Chunk::COUNT < 8) return 0; // cannot be pooled

    cnt = (cnt - usable) * N;                    // need amount to byte
    cnt = global::bit_align(cnt, CHUNK) / CHUNK; // align

    size_t generated = 0;
    while(generated < cnt) {
        Chunk* chunk = generate();
        if(!chunk) {
            return generated; // failed
        }
        empty.push(chunk); // insert

        ++generated;
    }
    return generated * Chunk::COUNT; // create count
}

template<size_t N>
size_t Allocator<N>::shrink() {
    size_t cnt = 0;
    Chunk* del = empty.next(); // pop

    while(del != nullptr) {
        Chunk* temp = empty.next(); // pop
        destroy(del);               // delete
        del = temp;                 // set next
        ++cnt;
    }
    return cnt;
}

template<size_t N> auto Allocator<N>::generate() noexcept -> Chunk* {
    Chunk* ptr = global::pal_valloc<Chunk>(CHUNK / 1024); // to KiB

    if(ptr) {
        new(ptr) Chunk;

        ptr->meta.outer = this; // init

        usable += Chunk::COUNT; // add
    }

    return ptr;
}

template<size_t N> void Allocator<N>::destroy(Chunk* in) noexcept {
    in->~Chunk();
    global::pal_vfree(in, CHUNK / 1024);

    usable += Chunk::COUNT;
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
