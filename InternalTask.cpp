// InternalTask.cpp - Internal task implementation
//
// Distributed under the Boost Software License, Version 1.0.
// See LICENSE_1_0.txt for details.
//


#include "InternalTask.h"
#include <assert.h>

namespace parallel {

InternalTask::InternalTask(TaskCompletion *comp)
    : completion(comp), _affinity(0xffffffff) {
  assert(uintptr_t(completion) < 0x0000ffffffffffff);
}
}
