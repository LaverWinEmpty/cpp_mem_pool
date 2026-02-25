#ifndef GLOBAL_PAL_HPP
#define GLOBAL_PAL_HPP

#include <new>

#include "internal/config.h"
#include "internal/target.h"

// WIN libraries
#if CHECK_TARGET(OS_WINDOWS)
#    include <intrin.h>
#endif

// POSIX libraries
#if CHECK_TARGET(OS_POSIX)
#    include <sys/mman.h>
#endif

#include "bit.hpp"

/**************************************************************************************************
 * Platform Abratction Layer PREFIX: pal
 **************************************************************************************************/
namespace global {

/**
 * @brief current thread id
 */
// static thread_local const std::thread::id THIS_THREAD = std::this_thread::get_id();

/**
 * @brief CPU hint for spin-wait loops
 */
CXX_FORCE_INLINE void pal_pause() noexcept;

/**
 * @brief call VirtualAlloc or mmap
 *
 * @tparam T type of the returned pointer
 * @param [in] byte  allocate size, value will be aligned to 16KiB, for follow Apple policy
 * @param [in] align address alignment, value will be aligned to 64KiB, follow Windows policy
 * @return a chunk whose address is aligned
 */
template<typename T = void> T* pal_valloc(size_t byte = PAL_PAGE, size_t align = PAL_BOUNDARY) noexcept;

/**
 * @brief call VirtualFree or unmap
 * @param [in] ptr  pointer from valloc
 * @param [in] byte OPTIONAL: need only POSIX: default 16384, same size used when calling valloc
 */
void pal_vfree(void* ptr, size_t byte = 16384) noexcept;

} // namespace global

//! @NOTE: like as "Windows.h"

#if CHECK_TARGET(OS_WINDOWS)
extern "C" {
    //__declspec(dllimport) void* __stdcall GetCurrentProcess();
    //__declspec(dllimport) void* __stdcall GetModuleHandleA(const char*);
    //__declspec(dllimport) void* __stdcall GetProcAddress(void*, const char*);

    //__declspec(dllimport) void*  __stdcall VirtualAlloc(void*, size_t, uint32_t, uint32_t);
    //__declspec(dllimport) int    __stdcall VirtualFree(void*, size_t, uint32_t);
    //__declspec(dllimport) size_t __stdcall VirtualQuery(void*, void*, size_t);
}
#endif

#include "pal.ipp"
#endif
