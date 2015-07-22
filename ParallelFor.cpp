// ParallelFor - Parallel for loop implementation
//
// Distributed under the Boost Software License, Version 1.0.
// See LICENSE_1_0.txt for details.
//

#include "ParallelFor.h"
#include "TaskPool.h"
#include "SpinMutex.h"
#include <algorithm>

namespace parallel {

InternalForTask::InternalForTask(TaskCompletion *completion,
                                 const ForTask *forTask, const Range &r)
    : InternalTask(completion), task(forTask), range(r) {}

bool InternalForTask::run(WorkerThread *thread) {
  const bool ok = task->do_range(thread, range);
  delete this;
  return ok;
}

InternalTask *InternalForTask::split(WorkerThread *thread) {
  if (range.end - range.begin < 2 * range.granularity)
    return nullptr;

  const int cutIndex = (range.begin + range.end) / 2;

  InternalForTask *newTask = new InternalForTask(
      completion, task, Range(cutIndex, range.end, range.granularity));
  range.end = cutIndex;

  return newTask;
}

InternalTask *InternalForTask::partial_pop(WorkerThread *thread) {
  if (range.end - range.begin < 2 * range.granularity)
    return nullptr;

  const int cutIndex = range.begin + range.granularity;

  InternalForTask *newTask = new InternalForTask(
      completion, this->task, Range(range.begin, cutIndex, range.granularity));
  range.begin = cutIndex;

  return newTask;
}

bool InternalForTask::spread(TaskPool *pool) {
  int begin = this->range.begin;
  int end = this->range.end;
  const int granularity = this->range.granularity;

  const int size = range.end - range.begin;
  // Determine how many threads we can spread this task to,
  // limited by the number of threads available in the pool.
  const int count =
      std::min<int>(size / range.granularity, pool->thread_count());

  if (count == 0)
    return false;

  const int partSize = (end - begin) / count;
  const int restSize = (end - begin) - count * partSize;

  for (int threadIndex = 0; threadIndex < count; ++threadIndex) {
    const int size = partSize + int(restSize > threadIndex);

    InternalTask *task = 0;
    if (threadIndex == 0) {
      task = this;
      this->range = Range(begin, begin + size, granularity);
    } else {
      task = new InternalForTask(this->completion, this->task,
                                 Range(begin, begin + size, granularity));
    }

    WorkerThread *thread = &pool->threads[threadIndex];
    {
      spin_mutex::lock task_lock(thread->task_mutex);

      completion->set_busy(true);
      thread->tasks.resize(0);
      thread->tasks.push_back(task);
    }
    begin += size;
  }
  return true;
}

bool parallel_for(const ForTask *task, const Range &range) {
  return parallel_for(WorkerThread::current(), task, range);
}

bool parallel_for(WorkerThread *thread, const ForTask *task,
                  const Range &range) {
  TaskCompletion completion;
  InternalForTask *internalTask = new InternalForTask(&completion, task, range);

  thread->push_task(internalTask);
  thread->work_until_done(&completion);

  return true;
}
}
