// WorkerThread.h - Work stealing task scheduler
//
// Distributed under the Boost Software License, Version 1.0.
// See LICENSE_1_0.txt for details.
//

#pragma once

#include "SpinMutex.h"
#include "TaskQueue.h"
#include "Types.h"

#include <atomic>
#include <thread>
#include <vector>

namespace parallel {

struct InternalTask;
struct TaskPool;

struct CACHELINE_ALIGNED TaskCompletion {
  TaskCompletion() : _busy(0) {}

  bool busy() const { return _busy != 0; }
  void set_busy(bool isbusy) {
    if (isbusy) {
      ++_busy;
    } else {
      --_busy;
    }
  }

private:
  std::atomic<int> _busy;
};

struct WorkerThread {
  static WorkerThread *current();
  static int current_index();
  static void set_current(WorkerThread *thread);

  WorkerThread();
  WorkerThread(const WorkerThread &) = delete;
  WorkerThread &operator=(const WorkerThread &) = delete;

  bool start(TaskPool *pool);
  bool attach_to_this_thread(TaskPool *);

  void idle();
  void join();

  bool push_task(InternalTask *task);

  int worker_index() const;

  void work_until_done(TaskCompletion *);

private:
  /// Thread entry point.
  void run();

  void do_work(TaskCompletion *);

  bool push_task_internal(InternalTask *task);

  bool steal_tasks();
  bool give_up_some_work(WorkerThread *idleThread);

public:
  TaskCompletion *current_completion;
  std::atomic<bool> finished;

private:
  TaskQueue<MAX_TASKSPERTHREAD> tasks;
  TaskPool *taskpool;

  std::thread _thread;
};

} // namespace parallel
