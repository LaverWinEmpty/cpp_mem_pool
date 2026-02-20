#ifndef GLOBAL_PAL_HPP
#define GLOBAL_PAL_HPP

#include "internal/target.h"
#include "bit.hpp"

#if TARGET_OS == OS_WINDOWS
#    include <intrin.h>
#endif

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
CXX_INLINE void pal_pause() noexcept;

/**
 * @brief call VirtualAlloc or mmap
 *
 * @tparam T type of the returned pointer
 * @param [in] kb allocate size KiB, value will be aligned to 4
 * @return a chunk whose address is aligned to 64 KiB (nullptr if failed)
 */
template<typename T = void> T* pal_valloc(size_t kb = 64) noexcept;

/**
 * @brief call VirtualFree or unmap
 * @param [in] ptr pointer from valloc
 * @param [in] in  need only POSIX: default 64 (KiB), same size used when calling valloc
 */
void pal_vfree(void* in, size_t kb = 64) noexcept;

} // namespace global

//! @NOTE: like as "Windows.h"
extern "C" {
#if TARGET_OS == OS_WINDOWS
    __declspec(dllimport) void* __stdcall GetCurrentProcess();
    __declspec(dllimport) void* __stdcall GetModuleHandleA(const char*);
    __declspec(dllimport) void* __stdcall GetProcAddress(void*, const char*);

    __declspec(dllimport) void*  __stdcall VirtualAlloc(void*, size_t, uint32_t, uint32_t);
    __declspec(dllimport) int    __stdcall VirtualFree(void*, size_t, uint32_t);
    __declspec(dllimport) size_t __stdcall VirtualQuery(void*, void*, size_t);
#endif
}

#include "pal.ipp"
#endif
