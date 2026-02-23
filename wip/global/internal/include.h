#ifndef GLOBAL_INTERNAL_INCLUDE_H
#define GLOBAL_INTERNAL_INCLUDE_H

// C++ standard
#include <cstdint>
#include <cstdlib>

// macors
#include "target.h"

// WIN libraries
#if CHECK_TARGET(OS_WINDOWS)
#    include <intrin.h>
#endif

// POSIX libraries
#if CHECK_TARGET(OS_POSIX)
#    include <sys/mman.h>
#endif

#endif
