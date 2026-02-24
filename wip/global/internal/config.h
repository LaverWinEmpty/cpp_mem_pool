#ifndef GLOBAL_INTERNAL_CONFIG_H
#define GLOBAL_INTERNAL_CONFIG_H

namespace global {

static constexpr size_t PAL_PAGE     = 1 << 14; //!< 16 KiB: memory page allocate unit (multiple of)
static constexpr size_t PAL_BOUNDARY = 1 << 16; //!< 64 KiB: memory page alignment unit (power of 2)
static constexpr size_t PAL_HUGEPAGE = 1 << 21; //!<  2 MiB: memory page large baseline (multiple of)

} // namespace global
#endif
