#ifndef GLOBAL_BIT_HPP
#define GLOBAL_BIT_HPP

#include "internal/include.h"

/**************************************************************************************************
 * bit operatiotns PREFIX: bit
 **************************************************************************************************/
namespace global {

/**
 * @brief count trailing zeros
 *
 * @param [in] uint64_t
 * @return count 0 from LSB to MSB (-1 if input 0)
 */
constexpr int bit_ctz(uint64_t) noexcept;

/**
 * @brief ount leading zeros
 *
 * @param [in] in
 * @return count 0 from MSB to LSB (-1 if input 0)
 */
constexpr int bit_clz(uint64_t in) noexcept;

/**
 * @brief get next power of 2
 *
 * @param [in] in source value
 * @return power of 2 (1 if input 0)
 */
constexpr uint64_t bit_pow2(uint64_t in) noexcept;

/**
 * @brief calculate binary logarithm
 *
 * @param[in] in aligned value
 * @return exponent of 2 (-1 if input not aligned value)
 */
constexpr int bit_log2(uint64_t in) noexcept;

/**
 * @brief get padded value
 *
 * @param [in] in        value to pad
 * @param [in] alignment if 0 then adjust to 1
 * @return aligned value (-1 if input alignment is not aligned)
 */
constexpr uint64_t bit_align(uint64_t in, uint64_t alignment = sizeof(void*)) noexcept;

/**
 * @brief check is aligned
 *
 * @param [in] in        value to check
 * @param [in] alignment if 0 then check power of 2
 * @return false if input alignment is not aligned
 */
constexpr bool bit_aligned(uint64_t in, uint64_t alignment = 0) noexcept;

} // namespace global

#include "bit.ipp"
#endif
