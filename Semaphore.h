// Semaphore.h - Semaphore
//
// Distributed under the Boost Software License, Version 1.0.
// See LICENSE_1_0.txt for details.
//

#pragma once

#include <mutex>
#include <condition_variable>

namespace parallel {

struct semaphore {

  explicit semaphore(int cnt = 0) : _count(cnt) {}

  int count() {
    std::unique_lock<std::mutex> lock(_mutex);
    return _count;
  }

  void release(int i) {
    std::unique_lock<std::mutex> lock(_mutex);
    _count += i;

    _cond.notify_all();
  }

  void wait() {
    std::unique_lock<std::mutex> lock(_mutex);
    while (_count == 0) {
      _cond.wait(lock);
    }
    --_count;
  }

private:
  int _count;
  std::mutex _mutex;
  std::condition_variable _cond;
};
}
