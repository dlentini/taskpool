
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -Wall -Wextra -Wno-unused-parameter -Wglobal-constructors")

add_library(taskpool InternalTask.cpp TaskPool.cpp WorkerThread.cpp)
