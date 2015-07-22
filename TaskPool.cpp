// TaskPool.cpp - Task pool implementation
//
// Distributed under the Boost Software License, Version 1.0.
// See LICENSE_1_0.txt for details.
//

#include "TaskPool.h"

namespace parallel {

TaskPool::TaskPool()
    : main_completion(nullptr), shutting_down(false), workers_idle(true),
      _thread_count(0), _worker_count(0) {}

TaskPool::~TaskPool() { stop(); }

bool TaskPool::start(int nthreads) {
  shutting_down = false;
  workers_idle = false;
  main_completion = nullptr;

  if (nthreads <= 0) {
    nthreads = 4;
  }
  nthreads = std::min(MAX_THREADS, nthreads);
  _thread_count = nthreads;
  _worker_count = _thread_count - 1;

  threads[0].attach_to_this_thread(this);
  for (int i = 1; i < _thread_count; ++i) {
    threads[i].start(this);
  }

  wait_for_workers_to_be_ready();
  wake_workers();

  return true;
}

bool TaskPool::stop() {
  shutting_down = true;
  wake_workers();

  for (int i = 1; i < _thread_count; ++i) {
    while (!threads[i].finished) {
    }
  }

  for (int i = 1; i < _thread_count; ++i) {
    threads[i].join();
  }

  return true;
}

void TaskPool::wait_for_workers_to_be_ready() {
  for (int i = 0; i < _worker_count; ++i) {
    sleep_notification.wait();
  }

  workers_idle = true;
}

void TaskPool::wake_workers() {
  workers_idle = false;
  wake_up_call.release(_worker_count);
}
}
