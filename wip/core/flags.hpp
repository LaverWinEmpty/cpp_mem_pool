#ifndef CORE_MASK_HPP
#define CORE_MASK_HPP

#include "../mem/aligner.hpp"

namespace core {




//! @brief bit-flag
//! @param N bits
template<size_t N, bool = false> class Flags;

//! @brief USE AS POINTER: `Flags` base logic
template<bool BASE> class Flags<0, BASE> {
public:
    /**
     * @param  [in] idx index
     * @return this
     */
    void on(size_t idx);

public:
    /**
     *@param  [in] idx index
     *@return this
     */
    void off(size_t idx);

public:
    /**
     * @param  [in] idx index
     * @return this
     */
    void toggle(size_t idx);

public:
    /**
     * @param  [in] idx index
     * @return get flag state
     */
    bool check(size_t idx) const;

public:
    /**
     * @param  [in] bits 0 is inf loop
     * @return first index, -1 is not found
     */
    size_t find(uint64_t bits = 0) const;

public:
    /**
     * @param  [in] bits
     */
    void zero(uint64_t bits);

protected:
    uint64_t flags[1] = { 0 };
};

//! @brief Flags default logic (zero specialization)
template<> class Flags<1, true> : public Flags<0, true> {
private:
    using Flags<0, true>::find; // hide
    using Flags<0, true>::zero; // hide

public:
    /**
     * @return first index, -1 is not found
     */
    size_t next() const;

public:
    /**
     * @brief set 0
     */
    void reset();

public:
    Flags<0, true>* ptr();
};

//! @brief Flags default logic
template<size_t N> class Flags<N, true>: public Flags<1, true> {
public:
    /**
     * @return first index, -1 is not found
     */
    size_t next() const;

public:
    /**
     * @brief set 0
     */
    void reset();

private:
    /**
     * @brief bit-maks flags
     */
    uint64_t expanded[N - 1] = { 0 };
};

//! @brief Flags instance logic
template<size_t N>
class Flags<N, false>: public Flags<Aligner::count(N, 64), true> {
public:
    static constexpr size_t SIZE = N;
};

} // namespace core

#include "flags.ipp"
#endif
