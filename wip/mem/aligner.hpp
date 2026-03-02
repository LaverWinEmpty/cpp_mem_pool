#ifndef MEM_ALIGNER_HPP
#define MEM_ALIGNER_HPP
#include "../global/bit.hpp"
#include "../global/pal.hpp"
#include <cstddef>

//! @brief memory align utility, with Allocator Mix-in base class
class Aligner {
public:
    //! @brief align to pointer size (4 or 8 Byte)
    static constexpr size_t ptr(size_t in);

public:
    //! @brief align to page size (16 KiB)
    static constexpr size_t page(size_t in);

public:
    //! @brief align to allocation graularity size (64 KiB)
    static constexpr size_t boundary(size_t in);

public:
    //! @brief get next power of 2 (round up)
    static constexpr size_t ceil(size_t in);

public:
    //! @brief get previous power of 2 (round down)
    static constexpr size_t floor(size_t in);

public:
    //! @brief get value by ceiling division
    static constexpr size_t count(size_t in, size_t div);

protected:
    //! @brief get chunk size, guaranteed at least 15 blocks
    static constexpr size_t chunk(size_t block);

protected:
    //! @brief get block count per 1 slab
    static constexpr size_t blocks(size_t chunk, size_t block);

protected:
    //! @brief get block begin position, with `Header`
    static constexpr size_t offset(size_t chunk, size_t block);

protected:
    struct Binder; //!< protect code bloat: `Slab` <-> `Bin` binder
    struct Header; //!< protect code bloat: chunk metadata of `Slab`
    class List;    //!< protect code bloat: aligned pointer cache by free-list style (forward list)
    class Array;   //!< protect code bloat: aligned pointer cache by pointer array (stack)

protected:
    Aligner() = default;
};

#include "aligner.ipp"
#endif
