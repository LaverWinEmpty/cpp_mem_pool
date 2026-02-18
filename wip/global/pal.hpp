#ifndef GLOBAL_PAL_HPP
#define GLOBAL_PAL_HPP

#include "internal/target.h"

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

//! NEED: "Windows.h"
extern "C" {
#ifndef _WIN64
#    define MY_STDCALL __attribute__((stdcall))
#else
#    define MY_STDCALL
#endif

#if TARGET_OS == OS_WINDOWS
    __declspec(dllimport) void* __stdcall
#    if TARGET_BITS == BITS_64
        VirtualAlloc(void*, unsigned long long, unsigned long, unsigned long);
#    else
        VirtualAlloc(void*, unsigned long, unsigned long, unsigned long);
#    endif

    __declspec(dllimport) int __stdcall
#    if TARGET_BITS == BITS_64
        VirtualFree(void*, unsigned long long, unsigned long);
#    else
        VirtualFree(void*, unsigned long, unsigned long);
#    endif
#endif
}

#include "pal.ipp"
#endif
