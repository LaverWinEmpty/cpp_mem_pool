#ifndef GLOBAL_INTERNAL_INCLUDE_H
#define GLOBAL_INTERNAL_INCLUDE_H

// C++ standard
#include <cstdint>
#include <cstdlib>

// macors
#include "target.h"

// OS libraries
#if CHECK_TARGET(OS_WINDOWS)
#    include <intrin.h>
#endif

#endif
