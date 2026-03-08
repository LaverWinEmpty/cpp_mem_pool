#ifndef GLOBAL_BIT_HPP
#define GLOBAL_BIT_HPP

#include "internal/include.h"

/**************************************************************************************************
 * bit operatiotns PREFIX: bit
 **************************************************************************************************/
namespace global {

//! @brief like `std::popcount`
constexpr int bit_popcnt(uint64_t in) noexcept;

//! @brief  BitScanFirst, 0-based ffs
constexpr int bit_bsf(uint64_t in) noexcept;

//! @brief  BitScanReverse, 0-based fls
constexpr int bit_bsr(uint64_t in) noexcept;

//! @brief get next power of 2
constexpr uint64_t bit_ceil(uint64_t in) noexcept;

//! @brief get previous power of 2
constexpr uint64_t bit_floor(uint64_t in) noexcept;

/**
 * @brief get aligned value
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
