// InternalTask.h - Base class for Tasks
//
// Distributed under the Boost Software License, Version 1.0.
// See LICENSE_1_0.txt for details.
//

#pragma once

#include "Types.h"
#include <stdint.h>

namespace parallel {

struct TaskCompletion;
struct WorkerThread;
struct TaskPool;

struct InternalTask {
  InternalTask(TaskCompletion *);
  virtual ~InternalTask() {}
  InternalTask(const InternalTask &) = delete;
  InternalTask &operator=(const InternalTask &) = delete;

  FORCE_INLINE void set_affinity(uint32_t affinity) {
    this->_affinity = affinity;
  }
  FORCE_INLINE uint32_t affinity() const { return _affinity; }

  virtual bool run(WorkerThread *) = 0;
  virtual InternalTask *split(WorkerThread *) { return nullptr; }
  virtual InternalTask *partial_pop(WorkerThread *) { return nullptr; }
  virtual bool spread(TaskPool *) { return false; }

  TaskCompletion *completion;

private:
  uint32_t _affinity;
};
}
