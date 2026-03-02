#ifndef MEM_SLAB_HPP
#define MEM_SLAB_HPP

#include "aligner.hpp"
#include "../core/flags.hpp"
#include <cstddef>
#include <cstdint>
#include "bin.hpp"

template<size_t N, bool = false> class Slab;

template<size_t N> class Slab<N, false>: public Slab<Aligner::ptr(N), true> { };

//! @note: guaranteed at least 64KiB, memory overhead max 6.25%
template<size_t N> class Slab<N, true>: protected Aligner {
public:
    static constexpr size_t BLOCK = ptr(N);               // for safety
    static constexpr size_t CHUNK = chunk(BLOCK);         // block as chunk
    static constexpr size_t UNIT  = blocks(CHUNK, BLOCK); // block per chunk is 1

private:
    using State = core::Flags<UNIT>;
    struct Meta {
        Header meta;
        State  state;
    };
    struct Chunk: Meta {
        uint8_t data[CHUNK - sizeof(Meta)];
    };
    static constexpr size_t OFFSET = offset(CHUNK, BLOCK);

protected:
    using Cache = List;

public:
    //! @brief constructor, with grouping
    template<size_t X, typename = typename Bin<X>::GROUPABLE> Slab(Bin<X>*);

public:
    Slab() = default;
    ~Slab();

public:
    //! @brief allocate
    //! @tparam T constructor type
    void* acquire() noexcept;

public:
    //! @brief deallocate
    //! @tparam T deconstructor type
    void release(void*) noexcept;

public:
    //! @param [in] count the blocks to keep (lower bound)
    //! @return actual allocated blocks
    size_t reserve(size_t count);

public:
    //! @param [in] count the blocks to keep (upper bound)
    //! @return actual deallocated blocks
    size_t trim(size_t count = 0);

public:
    //! @brief all blocks move to free-list, like as reset of arena style pool
    void reclaim();

public:
    //! @brief force free all block
    void purge();

public:
    //! @return remainder cached blocks
    size_t usable() const;

protected:
    void* map() noexcept;
    void  unmap(void*) noexcept;

protected:
    Chunk* current = nullptr;
    size_t counter = 0;

private:
    Cache frees;
    Cache fulls;
    Cache partials;

private:
    Aligner* const OUTER = nullptr; //!< bound Bin<?>
    const Binder   binder;          //!< bound Bin<?> info
};

#include "slab.ipp"
#endif
