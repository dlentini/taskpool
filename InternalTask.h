// InternalTask.h - Base class for Tasks
//
// Distributed under the Boost Software License, Version 1.0.
// See LICENSE_1_0.txt for details.
//

#pragma once

namespace parallel {

struct TaskCompletion;
struct WorkerThread;

struct InternalTask {
  InternalTask(TaskCompletion *);
  virtual ~InternalTask() {}
  InternalTask(const InternalTask &) = delete;
  InternalTask &operator=(const InternalTask &) = delete;

  virtual bool run(WorkerThread *) = 0;

  TaskCompletion *completion;
};

} // namespace parallel
