// Async.h - Async function
//
// Distributed under the Boost Software License, Version 1.0.
// See LICENSE_1_0.txt for details.
//

#pragma once

#include "InternalTask.h"
#include "WorkerThread.h"

#include <functional>

namespace parallel {

struct AsyncTask : InternalTask {
  template <class Fn, class... Args>
  AsyncTask(Fn fn, Args... args)
      : InternalTask(WorkerThread::current()->current_completion),
        fnc(std::bind(fn, args...)) {}
  template <class Fn, class... Args>
  AsyncTask(Fn fn, TaskCompletion *cmpl, Args... args)
      : InternalTask(cmpl), fnc(std::bind(fn, args...)) {}

  bool run(WorkerThread *) final {
    fnc();
    delete this;
    return true;
  }
  std::function<void(void)> fnc;
};

template <class Fn, class... Args> void async(Fn &&fn, Args &&... args) {
  WorkerThread::current()->push_task(new AsyncTask(fn, args...));
}
}
