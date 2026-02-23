#ifndef GLOBAL_INTERNAL_CONFIG_H
#define GLOBAL_INTERNAL_CONFIG_H

namespace global {

static constexpr size_t PAL_PAGE     = 16384; //!< memory page allocate unit (multiple of) 16 KiB
static constexpr size_t PAL_BOUNDARY = 65536; //!< memory page alignment unit (power of 2) 64 KiB

} // namespace global
#endif
