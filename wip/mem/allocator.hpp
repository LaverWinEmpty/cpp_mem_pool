#ifndef MEM_ALLOCATOR_HPP
#define MEM_ALLOCATOR_HPP

#include "../core/mask.hpp"
#include "../global/pal.hpp"
#include "../global/num.hpp"
#include <cassert>
#include <cstdlib>
#include <utility>
#include <type_traits>

template<size_t, bool = false> class Allocator;

//! @brief non-aligned size allocator
template<size_t N> class Allocator<N, false>
    : public Allocator<global::bit_align(N, (N >= global::PAL_HUGEPAGE ? global::PAL_HUGEPAGE : sizeof(void*))), true> { };

//! @brief pre-aligned size allocator
template<size_t N, bool> class Allocator {
public:
    static constexpr size_t BLOCK = global::bit_align(N, sizeof(void*)); //!< alginment (for safety)

private:
    static constexpr bool WHOLE = BLOCK >= global::PAL_HUGEPAGE; //!< flag

private:
    struct Meta;  //!< metadata, header
    struct Chunk; //!< chunk
    struct List;  //!< chunk as node, single linked list
    struct Array; //!< chunk pointer vector (for huge)

private:
    using Stack = std::conditional_t<WHOLE, Array, List>; //!< List or Array selector
    
public:
    static constexpr size_t CHUNK =
        WHOLE ? BLOCK : // HUGE: fallback: 1 chunk as 1 block, with meta
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
    template<typename T = void> void release(T* ptr);

public:
    /**
     * @brief syscall: create chunks
     *
     * @param [in] cnt need block count
     * @return created blocks count
     */
    size_t reserve(size_t cnt);

public:
    /**
     * @brief syscall: destroy empty chunks
     *
     * @return destroyed chunks count
     */
    size_t shrink();

public:
    /**
     * @brief change all chunks to empty state
     * @throw when chunk in use on debug
     */
    void reset();

public:
    /**
     * @brief get remaind block count
     */
    size_t usable();

private:
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
