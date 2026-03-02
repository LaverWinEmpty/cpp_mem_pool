#ifndef MEM_BIN_HPP
#define MEM_BIN_HPP

#include "aligner.hpp"

template<size_t N, bool = false> class Bin;

template<size_t N> class Bin<N, false>: public Bin<Aligner::page(N), true> { };

//! @note: guaranteed at least 64KiB, memory overhead max 20%
template<size_t N> class Bin<N, true>: protected Aligner {
public:
    static constexpr size_t BLOCK = boundary(N); // for safety
    static constexpr size_t CHUNK = BLOCK;       // block as chunk
    static constexpr size_t UNIT  = 1;           // block per chunk is 1

protected:
    using Cache = Array;

public:
    //! @brief allocate
    //! @tparam T constructor type
    template<typename T = void> T* acquire();

public:
    //! @brief deallocate
    //! @tparam T deconstructor type
    template<typename T = void> void release(T*);

public:
    //! @param [in] count the blocks to keep (lower bound)
    //! @return actual allocated blocks
    size_t ensure(size_t count);

public:
    //! @param [in] count the blocks to keep (upper bound)
    //! @return actual deallocated blocks
    size_t trim(size_t count = 0);

public:
    //! @return remainder cached blocks
    size_t usable() const;

public:
    //! @brief NO-OP, it is for only `Slab`
    bool bind(void*);

private:
    Cache  frees;   //!< free-list
    Cache  fulls;   //!< full-list
    size_t counter; //!< usable blocks count

private:
    //! for `Slab` bind
    static constexpr struct Binder VIRTUAL = {
        [](void* self) { return static_cast<Bin<N>*>(self)->acquire(); },
        [](void* self, void* ptr) { static_cast<Bin<N>*>(self)->release(ptr); },
        CHUNK,
    };

public:
    //! for `Slab` bind
    const Binder& VTABLE = VIRTUAL;

public:
    //! for `Slab` bind
    using GROUPABLE = std::enable_if_t<global::bit_aligned(CHUNK)>;
};

#endif
