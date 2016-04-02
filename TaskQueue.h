// TaskQueue.h - Concurrent work queue
//
// Distributed under the Boost Software License, Version 1.0.
// See LICENSE_1_0.txt for details.
//
#pragma once

#include "SpinMutex.h"

namespace parallel {

struct InternalTask;

/// \brief Fixed sized concurrent task queue.
///
/// Push/Pop tasks from top of queue and give up tasks from bottom.
///
/// \param QUEUE_SIZE Capacity of the queue.
template <size_t QUEUE_SIZE> class TaskQueue {
public:
  TaskQueue() : size(0) {}
  TaskQueue(const TaskQueue &) = delete;
  TaskQueue &operator=(const TaskQueue &) = delete;

  /// Pop a task from top of the queue.
  InternalTask *pop() {
    spin_mutex::lock lock(mtx);
    if (size == 0)
      return nullptr;

    InternalTask *result = tasks[size - 1];
    --size;
    return result;
  }

  /// Push a task on top of the queue.
  bool push(InternalTask *task) {
    spin_mutex::lock lock(mtx);
    if (size == QUEUE_SIZE)
      return false;

    tasks[size] = task;
    ++size;
    return true;
  }

  /// Give up half of the tasks to the idle task queue.
  bool give_up_tasks(TaskQueue &idle) {
    spin_mutex::lock task_lock(mtx);
    if (size == 0)
      return false;

    spin_mutex::lock idle_lock(idle.mtx);
    if (idle.size)
      return false;

    // steal bottom half of my tasks (rounding up)
    const size_t steal_count = (size + 1) / 2;

    // move old tasks to other list
    InternalTask **p = idle.tasks;
    size_t i = 0;
    for (; i < steal_count; ++i) {
      *p++ = tasks[i];
    }
    idle.size = steal_count;

    // move remaining tasks down
    p = tasks;
    for (; i < size; ++i) {
      *p++ = tasks[i];
    }
    size -= steal_count;

    return true;
  }

  bool empty() {
    spin_mutex::lock lock(mtx);
    return size == 0;
  }

private:
  spin_mutex mtx;
  InternalTask *tasks[QUEUE_SIZE];
  size_t size;
};

} // namespace parallel
