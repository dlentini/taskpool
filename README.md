taskpool
========

Implementation of a randomized work stealing task scheduler.

How it works:

  A number of WorkerThreads executing Tasks from it's own TaskQueue.
  When the TaskQueue is empty the WorkerThread tries to steal some tasks
  from another threads TaskQueue.

  The TaskPool owns the WorkerThreads but don't have any important logic
  of it's own. The main part is inside WorkerThread and TaskQueue.

  InternalTask is the base class for tasks that get executed.

  Async contains helper functions to execute tasks.

How to use:

  parallel::TaskPool scheduler;
  scheduler.start(std::thread::hardware_concurrency());

  parallel::TaskCompletion completion;
  parallel::async(MyAsyncFunc, &completion);
  parallel::WorkerThread::current()->work_until_done(&completion);

References:

  https://software.intel.com/en-us/articles/do-it-yourself-game-task-scheduling
