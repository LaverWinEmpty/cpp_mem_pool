#ifndef CORE_MASK_HPP
#define CORE_MASK_HPP

#include "../global/bit.hpp"

namespace core {

//! @brief bit mask
template<size_t N> class Mask {
public:
    /**
     * @param  [in] idx index
     * @return this
     */
    Mask<N>& on(size_t idx);

public:
    /**
     *@param  [in] idx index
     *@return this
     */
    Mask<N>& off(size_t idx);

public:
    /**
     * @param  [in] idx index
     * @return this
     */
    Mask<N>& toggle(size_t idx);

public:
    /**
     * @param  [in] idx index
     * @return get flag state
     */
    bool check(size_t idx) const;

public:
    /**
     * @return first index, -1 is not found
     */
    size_t next() const;

private:
    /**
     * @brief bit-maks flags
     */
    uint64_t flags[N];
};

} // namespace core

#include "mask.ipp"
#endif
