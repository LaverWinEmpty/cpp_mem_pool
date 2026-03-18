#ifndef UTIL_BITSET_HPP
#define UTIL_BITSET_HPP

#include "../core/bitmask.hpp"

namespace util {

/**
 * @brief  Find First Zero function callbale C++17 bitset class
 * @tparam BIT bit count
 * @tparam T   bit operation unit type (integer)
 */
template<size_t BIT, typename T = unsigned int, bool = false> class Bitset;

//! @brief `Bitset` base class
//! @tparam SIZE BIT to Word size
template<size_t SIZE, typename Word> class Bitset<SIZE, Word, true> : protected core::Bitmask {
public:
    //! @pre   idx < BIT
    //! @param [in] idx index
    void on(size_t idx);

public:
    //! @pre   idx < BIT
    //! @param [in] idx index
    void off(size_t idx);

public:
    //! @pre   idx < BIT
    //! @param [in] idx index
    void toggle(size_t idx);

public:
    //! @param  [in] idx index
    //! @return get flag state
    bool check(size_t idx) const;
    
public:
    //! @brief  Find First Zero
    //! @return first index, -1 is haven't slot
    size_t next();
    
public:
    //! @brief popcount
    size_t count();

protected:
    Word flags[SIZE] = { 0 };
};

}

#include "bitset.ipp"
#endif
