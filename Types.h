// Types.h - Defines
//
// Distributed under the Boost Software License, Version 1.0.
// See LICENSE_1_0.txt for details.
//

#pragma once

namespace parallel {

#define FORCE_INLINE inline
#define CACHELINE_SIZE 16
#define CACHELINE_ALIGNED alignas(CACHELINE_SIZE)

#define MAX_THREADS 32
#define MAX_TASKSPERTHREAD 256

} // namespace parallel
