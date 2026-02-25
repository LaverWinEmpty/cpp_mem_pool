#ifndef GLOBAL_INTERNAL_TARGET_H
#define GLOBAL_INTERNAL_TARGET_H

/**************************************************************************************************
 * DECLARE FLAGS                                                                                  *
 **************************************************************************************************/

// Endianess
#define ENDIAN_BIG    (0x01 << 0) //!< Endianess big
#define ENDIAN_LITTLE (0x02 << 0) //!< Endianess little
#define ENDIAN_MIDDLE (0x04 << 0) //!< Endianess middle (UNUSED)
// Bitness
#define BITS_16  (0x10 << 0) //!< Bitness 16,  used as bits size (UNUSED)
#define BITS_32  (0x20 << 0) //!< Bitness 32,  used as bits size
#define BITS_64  (0x40 << 0) //!< Bitness 64,  used as bits size
#define BITS_128 (0x80 << 0) //!< Bitness 128, used as bits size (UNUSED)
// Architectures
#define ARCH_X86 (0x01 << 8) //!< Instruction Set Architecture
#define ARCH_ARM (0x02 << 8) //!< Instruction Set Architecture
// Compilers
#define COMP_CLANG (0x10 << 16) //!< Compiler LLVM Clang
#define COMP_GCC   (0x20 << 16) //!< Compiler GNU GCC
#define COMP_MSVC  (0x40 << 16) //!< Compiler MSVC CL
// OS
#define OS_POSIX   (0x10 << 24) //!< OS using POSIX API
#define OS_WINDOWS (0x20 << 24) //!< OS using Win32 API
// CPP
#define CPP_98 199711
#define CPP_03 199711
#define CPP_11 201103
#define CPP_14 201402
#define CPP_17 201703
#define CPP_20 202002
#define CPP_23 202302
// CPP_VER
#ifndef CPP_VER
#    if defined(_MSC_VER)
#        define CPP_VER _MSVC_LANG
#    else
#        define CPP_VER __cplusplus
#    endif
#endif

/**************************************************************************************************
 * SET TARGET FLAGS PREFIX: TARGET_                                                               *
 **************************************************************************************************/

// OS
#ifndef TARGET_OS
#    if defined(__unix__) || defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__)
#        define TARGET_OS OS_POSIX
#    elif defined(_WIN32)
#        define TARGET_OS OS_WINDOWS
#    else
#        error Unknown OS.
#    endif
#endif

// Compiler
#ifndef TARGET_COMP
#    if defined(__clang__)
#        define TARGET_COMP COMP_CLANG
#    elif defined(__GNUC__)
#        define TARGET_COMP COMP_GCC
#    elif defined(_MSC_VER)
#        define TARGET_COMP COMP_MSVC
#    else
#        error Unknown Compiler.
#    endif
#endif

// ISA
#ifndef TARGET_ARCH
#    if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
#        define TARGET_ARCH ARCH_X86
#    elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
#        define TARGET_ARCH ARCH_ARM
#    else
#        error Unknown Architecture.
#    endif
#endif

// Bitness
#ifndef TARGET_BITS
#    if defined(__SIZEOF_POINTER__)
#        if __SIZEOF_POINTER__ == 8
#            define TARGET_BITS BITS_64
#        elif __SIZEOF_POINTER__ == 4
#            define TARGET_BITS BITS_32
#        endif
#    endif
// Fallback
#    ifndef TARGET_BITS
#        if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
#            define TARGET_BITS BITS_64
#        elif defined(_WIN32) || defined(__i386__) || defined(__arm__)
#            define TARGET_BITS BITS_32
#        else
#            error Unknown Bitness.
#        endif
#    endif
#endif

// Endianess
#ifndef TARGET_ENDIAN
// Calng, GCC, etc.
#    if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)
#        if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#            define TARGET_ENDIAN ENDIAN_LITTLE
#        elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#            define TARGET_ENDIAN ENDIAN_BIG
#        endif
// Windows
#    elif defined(_WIN32) || defined(_MSC_VER)
#        define TARGET_ENDIAN ENDIAN_LITTLE
// ARM Bi-endian
#    elif defined(__LITTLE_ENDIAN__) || defined(_LITTLE_ENDIAN)
#        define TARGET_ENDIAN ENDIAN_LITTLE
#    elif defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN)
#        define TARGET_ENDIAN ENDIAN_BIG
// x86, etc.
#    elif defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
#        define TARGET_ENDIAN ENDIAN_LITTLE
#    else
#        error Unknown Endianess.
#    endif
#endif

// Flag set
#ifndef TARGET
#    define TARGET (TARGET_ARCH | TARGET_BITS | TARGET_COMP | TARGET_ENDIAN | TARGET_OS)
#endif

// check target
#ifndef CHECK_TARGET
#    define CHECK_TARGET(FLAGS) ((TARGET & (FLAGS)) == (FLAGS))
#endif

/**************************************************************************************************
 * STATE FLAGS PREFIX: IS_                                                                        *
 **************************************************************************************************/

// debug mode
#ifndef IS_DEBUG
#    if defined(NDEBUG)
#        define IS_DEBUG 0
#    else
#        define IS_DEBUG 1
#    endif
#endif

/**************************************************************************************************
 * ATTRIBUTE PREFIX: CXX_                                                                         *
 **************************************************************************************************/

// has bultin
#ifndef CXX_HAS_BUILTIN
#    ifdef __has_builtin
#        define CXX_HAS_BUILTIN(x) __has_builtin(x)
#    else
#        define CXX_HAS_BUILTIN(x) 0 // false
#    endif
#endif

// is constant evaluated keyword
#ifndef CXX_IS_CONSTANT_EVALUATED
#    if CXX_HAS_BUILTIN(__builtin_is_constant_evaluated) || __GNUC__ >= 9 || _MSC_VER >= 1925
#        define CXX_IS_CONSTANT_EVALUATED() __builtin_is_constant_evaluated() // GCC 9.1+ / Clang 9.0+
#    else
#        define CXX_IS_CONSTANT_EVALUATED() false
#    endif
#endif

// launder keyword
#ifndef CXX_LAUNDER
#    if CXX_HAS_BUILTIN(__builtin_launder) || (TARGET_COMP == COMP_GCC && __GNUC__ >= 7) // GCC 7.1+
#        define CXX_LAUNDER(x) __builtin_launder(x)
#    else
#        define CXX_LAUNDER(x) (x)
#    endif
#endif

// force inline (prefix) keyword
#ifndef CXX_FORCE_INLINE
#    if TARGET_COMP & COMP_MSVC
#        define CXX_FORCE_INLINE __forceinline
#    elif TARGET_COMP & (COMP_CLANG | COMP_GCC)
#        define CXX_FORCE_INLINE __attribute__((always_inline)) inline // GCC 3.1+
#    else
#        define CXX_INLINE inline
#    endif
#endif

// no enter hint
#if CXX_HAS_BUILTIN(__builtin_unreachable) || (TARGET_COMP == COMP_GCC && __GNUC__ >= 5) // GCC 4.5+
    #define CXX_UNREACHABLE() __builtin_unreachable()
#elif TARGET_COMP == MSVC
    #define CXX_UNREACHABLE() __assume(0)
#else
    #define CXX_UNREACHABLE() abort()
#endif

//
#ifndef CXX_UNINIT_BEGIN
#    if TARGET_COMP & COMP_MSVC
#        define CXX_UNINIT_BEGIN __pragma(warning(push)) __pragma(warning(disable: 4701))
#    elif TARGET_COMP & (COMP_CLANG | COMP_GCC)
#        define CXX_UNINIT_BEGIN _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wuninitialized\"")
#    else
#        define CXX_UNINIT_BEGIN
#    endif
#endif

//
#ifndef CXX_UNINIT_END
#    if TARGET_COMP & COMP_MSVC
#        define CXX_UNINIT_END __pragma(warning(pop))
#    elif TARGET_COMP & (COMP_CLANG | COMP_GCC)
#        define CXX_UNINIT_END _Pragma("GCC diagnostic pop")
#    else
#        define CXX_UNINIT_END
#    endif
#endif

#endif
