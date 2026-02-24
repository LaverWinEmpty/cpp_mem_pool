#ifndef MEM_ALLOCATOR_HPP
#define MEM_ALLOCATOR_HPP

#include "../core/mask.hpp"
#include "../global/pal.hpp"
#include "../global/num.hpp"
#include <cassert>
#include <cstdlib>
#include <utility>
#include <type_traits>

template<size_t = 0, typename = void> class Allocator;

//! @brief Allocator size aligner
//! @note  N = 0 is not allowed
//!        but declaration is allowed
//!        and use it as a helper
template<typename T> class Allocator<0, T> {
    Allocator() = delete;
public:
    static constexpr size_t sizer(size_t n) {
        if constexpr(std::is_same_v<T, void>) {
            return 0; // [visible confusion]
        }
        return global::num_align(sizeof(T), global::bit_pow2(n));
    }
};

//! @brief non-aligned size allocator
template<size_t N> class Allocator<N, std::enable_if_t<(N % sizeof(void*) != 0)>>
    : public Allocator<global::num_align(N, sizeof(void*))> { };

//! @brief pre-aligned size allocator
template<size_t N, typename> class Allocator {
public:
    static constexpr size_t BLOCK = global::bit_align(N, sizeof(void*)); // alginment

private:
    static constexpr bool HUGE = BLOCK >= global::PAL_HUGEPAGE;

private:
    struct Meta;   //!< metadata, header
    struct Chunk;  //!< chunk
    struct List;   //!< chunk single linked list
    struct Vector; //!< chunk pointer vector (for huge)

public:
    static constexpr size_t CHUNK =
        HUGE ? BLOCK : // HUGE: fallback: 1 chunk as 1 block, with meta
            (global::bit_pow2(N * 15) <= global::PAL_BOUNDARY ? global::PAL_BOUNDARY : // SMALL: fixed 64KiB, default
                 (global::bit_pow2(N * 15)) // MEDIUM: at least 15 guaranteed, for 4KiB based on 64KiB
            );
    static constexpr size_t UNIT = Chunk::COUNT;

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
    using Stack = std::conditional_t<BLOCK < global::PAL_HUGEPAGE, List, Vector>;

    Stack full;    //!< chunks using block is 0
    Stack empty;   //!< chunks using block is full
    Stack partial; //!< chunks using block is ?

private:
    Chunk* current = nullptr; //!< using chunk
    size_t counter = 0;       //!< usable block counter

private:
    //! @brief syscall allocate
    Chunk* generate() noexcept;

private:
    //! @brief syscall deallocate
    void destroy(Chunk*) noexcept;
};

#include "allocator.ipp"
#endif
