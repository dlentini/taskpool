// WorkerThread.cpp - Work stealing task scheduler
//
// Distributed under the Boost Software License, Version 1.0.
// See LICENSE_1_0.txt for details.
//

#include "WorkerThread.h"
#include "TaskPool.h"
#include "InternalTask.h"
#include <algorithm>
#include <atomic>
#include <assert.h>

namespace parallel {

namespace {
__thread WorkerThread *current_worker = nullptr;
}

WorkerThread::WorkerThread()
    : current_completion(nullptr), taskpool(nullptr), finished(false) {
  finished = false;
  tasks.reserve(MAX_TASKSPERTHREAD);
}

WorkerThread *WorkerThread::current() { return current_worker; }

int WorkerThread::current_index() {
  assert(current_worker);
  return current_worker->worker_index();
}

void WorkerThread::set_current(WorkerThread *thread) {
  current_worker = thread;
}

int WorkerThread::worker_index() const {
  assert(taskpool);
  return static_cast<int>(this - taskpool->threads);
}

uint32_t WorkerThread::affinity_mask() const { return 1 << worker_index(); }

bool WorkerThread::start(TaskPool *pool) {
  taskpool = pool;
  finished = false;
  current_completion = nullptr;

  _thread = std::move(std::thread(&WorkerThread::run, this));

  return true;
}

bool WorkerThread::attach_to_this_thread(TaskPool *pool) {
  spin_mutex::lock task_lock(task_mutex);

  taskpool = pool;
  assert(tasks.empty());
  finished = false;
  current_worker = this;

  return true;
}

void WorkerThread::idle() {
  taskpool->sleep_notification.release(1);
  taskpool->wake_up_call.wait();
}

void WorkerThread::join() {
  if (_thread.joinable()) {
    _thread.join();
  }
}

void WorkerThread::run() {
  assert(taskpool);
  WorkerThread::set_current(this);

  while (true) {
    idle();

    if (taskpool->shutting_down)
      break;

    while (taskpool->main_completion.load()) {
      do_work(nullptr);
      if (taskpool->shutting_down) {
        break;
      }
    }
  }
  finished = true;
}

void WorkerThread::do_work(TaskCompletion *expected) {
  // NOTE:
  // If expected is NULL, then we'll work until there is nothing left to do.
  // This
  // is normally happening only in the case of a worker's thread loop (above).

  // if it isn't NULL, then it means the caller is waiting for this particular
  // thing
  // to complete (and will want to carry on something once it is). We will do
  // our work
  // and steal some until the condition happens. This is normally happening when
  // as
  // part of Work_until_done (below)

  // NOTE: This method needs to be reentrant (!)
  // A task can be spawing more tasks and may have to wait for their completion.
  // So, as part of our task->run() we can be called again, via the
  // work_until_done
  // method, below.

  do {
    InternalTask *task = pop_task();
    TaskCompletion *last_completion = nullptr;

    while (task) {

      last_completion = current_completion;
      current_completion = task->completion;

      task->run(this);

      current_completion->set_busy(false);
      current_completion = last_completion;

      if (expected && !expected->busy())
        return;

      task = pop_task();
    }

    if (!taskpool->main_completion.load())
      return;

  } while (steal_tasks() || (expected && expected->busy()));
}

void WorkerThread::work_until_done(TaskCompletion *completion) {
  if (completion->busy()) {
    do_work(completion);
  }

  assert(!completion->busy());

  if (taskpool->main_completion.compare_exchange_strong(completion, nullptr)) {
    // This is the root task. As this is finished, the scheduler can go idle.
    // What happens next: (eventually,) each worker thread will see that there
    // is no main completion any more and go idle waiting for semaphore to
    // signal
    // that new work nees to be done (see CWorkerThread::ThreadProc)
  }
}

InternalTask *WorkerThread::pop_task() {
  spin_mutex::lock task_lock(task_mutex);
  if (tasks.empty())
    return nullptr;

  InternalTask *task = tasks.back();
  assert(task);
  assert(task->completion);

  InternalTask *partial = task->partial_pop(this);
  if (partial) {
    task->completion->set_busy(true);
    return partial;
  }
  tasks.pop_back();
  return task;
}

bool WorkerThread::push_task(InternalTask *task) {
  assert((task->affinity() & affinity_mask()) != 0);
  if (push_task_internal(task)) {
    return true;
  }

  task->run(this);
  return false;
}

bool WorkerThread::push_task_internal(InternalTask *task) {
  if (taskpool->main_completion.load() == nullptr) {
    taskpool->wait_for_workers_to_be_ready();

    if (task->spread(taskpool)) {
      taskpool->main_completion = task->completion;
      taskpool->wake_workers();
      return true;
    }
  }

  {
    spin_mutex::lock lock(task_mutex);
    if (tasks.size() >= MAX_TASKSPERTHREAD) {
      return false;
    }

    task->completion->set_busy(true);
    tasks.push_back(task);
  }

  TaskCompletion *null = nullptr;
  if (taskpool->main_completion.compare_exchange_strong(null,
                                                        task->completion)) {
    taskpool->main_completion = task->completion;
    taskpool->wake_workers();
  }

  // if (taskpool->main_completion.load() == nullptr) {
  //	taskpool->main_completion = task->completion;
  //	taskpool->wake_workers();
  //}

  return true;
}

bool WorkerThread::steal_tasks() {
  const size_t nthreads = taskpool->thread_count();
  const size_t start = rand() % nthreads;

  for (size_t i = 0; i < nthreads; ++i) {
    WorkerThread &other = taskpool->threads[(start + i) % nthreads];
    if (&other != this) {
      if (other.give_up_some_work(this)) {
        return true;
      }

      spin_mutex::lock task_lock(task_mutex);
      if (!tasks.empty()) {
        return true;
      }
    }
  }
  return false;
}

bool WorkerThread::give_up_some_work(WorkerThread *idle_thread) {
  spin_mutex::lock task_lock(task_mutex);
  if (tasks.empty())
    return false;

  spin_mutex::lock idle_lock(idle_thread->task_mutex);
  if (!idle_thread->tasks.empty())
    return false;

  if (tasks.size() == 1) {
    assert(!tasks.empty());
    InternalTask *split = tasks.front()->split(idle_thread);
    if (split) {
      split->completion->set_busy(true);
      idle_thread->tasks.push_back(split);
      return true;
    }
  }

  InternalTask *buf[MAX_TASKSPERTHREAD];
  const size_t n = tasks.size();

  std::copy(tasks.begin(), tasks.end(), buf);
  tasks.resize(0);

  for (size_t i = 0; i < n; ++i) {
    InternalTask *task = buf[i];

    if (idle_thread->tasks.size() <
        tasks.size()) { // Don't steal too many tasks.
      if ((task->affinity() & idle_thread->affinity_mask()) != 0) {
        idle_thread->tasks.push_back(task);
        continue;
      }
    }
    tasks.push_back(task);
  }

  return !idle_thread->tasks.empty();
}

}
