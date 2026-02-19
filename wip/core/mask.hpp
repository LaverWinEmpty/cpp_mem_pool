#ifndef CORE_MASK_HPP
#define CORE_MASK_HPP

#include "../global/bit.hpp"

namespace core {

//! @brief bit mask
template<size_t N> class Mask {
public:
    /**
     * @param  [in] size_t index
     * @return this
     */
    Mask<N>& on(uint64_t);

public:
    /**
     *@param  [in] size_t index
     *@return this
     */
    Mask<N>& off(uint64_t);

public:
    /**
     * @param  [in] size_t index
     * @return this
     */
    Mask<N>& toggle(uint64_t);

public:
    /**
     * @param  [in] size_t index
     * @return get flag state
     */
    bool check(uint64_t) const;

public:
    /**
     * @return get flags count
     */
    size_t count() const;

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
