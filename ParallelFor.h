// ParallelFor.h - Simple parallel_for loop
//
// Distributed under the Boost Software License, Version 1.0.
// See LICENSE_1_0.txt for details.
//

#pragma once

#include "InternalTask.h"
#include "WorkerThread.h"

namespace parallel {

struct Range {
  Range() {}
  Range(const Range &rh) {
    begin = rh.begin;
    end = rh.end;
    granularity = rh.granularity;
  }

  Range(uint32_t begin_, uint32_t end_)
      : begin(begin_), end(end_), granularity(1) {}

  Range(uint32_t begin_, uint32_t end_, uint32_t granularity_)
      : begin(begin_), end(end_), granularity(granularity_) {}

public:
  uint32_t begin;
  uint32_t end;
  uint32_t granularity;
};

struct ForTask {
  virtual bool do_range(WorkerThread *thread, const Range &range) const = 0;
};

struct InternalForTask : public InternalTask {
  InternalForTask(TaskCompletion *completion, const ForTask *forTask,
                  const Range &r);

  bool run(WorkerThread *thread) override;
  InternalTask *split(WorkerThread *thread) override;
  InternalTask *partial_pop(WorkerThread *thread) override;
  bool spread(TaskPool *pool) override;

  const ForTask *task;
  Range range;
};

template <typename Fn> struct ForTaskWithFn : public ForTask {
  ForTaskWithFn(Fn func) : fn(func) {}

  bool do_range(WorkerThread *thread, const Range &range) const {
    fn(range);
    return true;
  }

  Fn fn;
};

template <typename Fn> inline bool parallel_for(const Range &range, Fn fn) {
  WorkerThread *thread = WorkerThread::current();

  ForTaskWithFn<Fn> task(fn);
  TaskCompletion completion;
  InternalForTask *internalTask =
      new InternalForTask(&completion, &task, range);

  thread->push_task(internalTask);
  thread->work_until_done(&completion);

  return true;
}

bool parallel_for(const ForTask *task, const Range &range);
bool parallel_for(WorkerThread *thread, const ForTask *task,
                  const Range &range);
}
