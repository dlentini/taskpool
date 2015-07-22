// WorkerThread.h - Work stealing task scheduler
//
// Distributed under the Boost Software License, Version 1.0.
// See LICENSE_1_0.txt for details.
//

#pragma once

#include "SpinMutex.h"
#include <atomic>
#include <thread>
#include <vector>

namespace parallel {

const int MAX_THREADS = 32;
const int MAX_TASKSPERTHREAD = 256;

struct InternalTask;
struct TaskPool;

struct TaskCompletion {
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
  void idle();

  void join();

  bool attach_to_this_thread(TaskPool *);

  InternalTask *pop_task();
  bool push_task(InternalTask *task);
  bool push_task_internal(InternalTask *task);

  int worker_index() const;
  uint32_t affinity_mask() const;

  void work_until_done(TaskCompletion *);
  void do_work(TaskCompletion *);

  bool steal_tasks();
  bool give_up_some_work(WorkerThread *idleThread);

protected:
  void run();

public:
  std::vector<InternalTask *> tasks;
  TaskCompletion *current_completion;
  TaskPool *taskpool;

  spin_mutex task_mutex;

  std::atomic<bool> finished;

private:
  std::thread _thread;
};
}
