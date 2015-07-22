// fParallelSort.hh - Parallel merge sort
//
// Distributed under the Boost Software License, Version 1.0.
// See LICENSE_1_0.txt for details.
//

#pragma once

#include "InternalTask.h"
#include "WorkerThread.h"

#include <cstdlib>
#include <cassert>

namespace parallel {

template <class T, class Comparator, int MinItemCountPerTask>
class SorterInternalTask;

template <class T, class Comparator, int MinItemCountPerTask> class Sorter {
public:
  Sorter(T *pData, unsigned Count) {
    m_pData = pData;
    m_Count = Count;
    m_pToFree = malloc(sizeof(T) * Count);
    m_pTemp = (T *)m_pToFree;
  }

  Sorter(T *pData, unsigned Count, T *pTemp) {
    m_pData = pData;
    m_Count = Count;
    m_pToFree = NULL;
    m_pTemp = pTemp;
  }

  ~Sorter() {
    if (m_pToFree)
      free(m_pToFree);
  }

  void Sort();
  void Sort_Serial(); /* ie not using task scheduler */

public:
  T *m_pData;
  T *m_pTemp;
  void *m_pToFree;
  unsigned m_Count;
};

template <class T, class Comparator, int MinItemCountPerTask>
class SorterInternalTask : public InternalTask {
public:
  enum eTaskType {
    TaskType_Sort,       /* sort the buffer passed	*/
    TaskType_MergeFront, /* merge the "front" half	*/
    TaskType_MergeBack   /* merge the "back" half	*/
  };

  SorterInternalTask(TaskCompletion *pCompletion) : InternalTask(pCompletion) {}

  void Init(Sorter<T, Comparator, MinItemCountPerTask> *pSorter, int Start,
            int Count, eTaskType TaskType = TaskType_Sort) {
    m_pSorter = pSorter;
    m_TaskType = TaskType;
    m_Start = Start;
    m_Count = Count;
  }

  void Merge(int iA, int nA, int iB, int nB) {
    T *p;
    T *pData = m_pSorter->m_pData;
    T *pTemp = m_pSorter->m_pTemp;
    int iC, nC;

    /* Find first item not already at its place */
    while (nA) {
      if (!Comparator::isCorrectOrder(pData[iA], pData[iB]))
        break;

      iA++;
      nA--;
    }

    if (!nA)
      return /* already sorted */;

    /* merge what needs to be merged in pTemp */
    iC = iA;
    p = &pTemp[iA];

    *p++ = pData[iB++]; /* first one is from B */
    nB--;

    while (nA && nB) {
      if (Comparator::isCorrectOrder(pData[iA], pData[iB])) {
        *p++ = pData[iA++];
        nA--;
      } else {
        *p++ = pData[iB++];
        nB--;
      }
    }

    /* if anything remains in B, it already is in correct order inside pData */
    /* if anything is left in A, it needs to be moved */
    memcpy(p, &pData[iA], nA * sizeof(T));
    p += nA;

    /* copy what has changed position back from pTemp to pData */
    nC = int(p - &pTemp[iC]);
    memcpy(&pData[iC], &pTemp[iC], nC * sizeof(T));
  }

  void Swap(T &A, T &B) {
    T C;

    C = A;
    A = B;
    B = C;
  }

  void Sort(int start, int count) {
    int half;

    switch (count) {
    /* case 0:				*/
    /* case 1: 				*/
    /* {					*/
    /* 	ASSERT("WTF?");		*/
    /* 	return;				*/
    /* }					*/

    case 2: {
      T *pData = m_pSorter->m_pData + start;

      if (!Comparator::isCorrectOrder(pData[0], pData[1]))
        Swap(pData[0], pData[1]);

    } break;

    case 3: {
      T *pData = m_pSorter->m_pData + start;
      T tmp;

      int k = (Comparator::isCorrectOrder(pData[0], pData[1]) ? 1 : 0) +
              (Comparator::isCorrectOrder(pData[0], pData[2]) ? 2 : 0) +
              (Comparator::isCorrectOrder(pData[1], pData[2]) ? 4 : 0);

      switch (k) {
      case 7: /* ABC */
        break;
      case 3: /* ACB */
        Swap(pData[1], pData[2]);
        break;
      case 6: /* BAC */
        Swap(pData[0], pData[1]);
        break;
      case 1: /* BCA */
        tmp = pData[0];
        pData[0] = pData[2];
        pData[2] = pData[1];
        pData[1] = tmp;
        break;
      case 4: /* CAB */
        tmp = pData[0];
        pData[0] = pData[1];
        pData[1] = pData[2];
        pData[2] = tmp;
        break;
      case 0: /* CBA */
        Swap(pData[0], pData[2]);
        break;
      default:
        assert(false);
      }

      if (!Comparator::isCorrectOrder(pData[0], pData[1])) {
        if (!Comparator::isCorrectOrder(pData[1], pData[2])) {
          Swap(pData[0], pData[2]);
        } else
          Swap(pData[0], pData[1]);
      }

      if (!Comparator::isCorrectOrder(pData[1], pData[2]))
        Swap(pData[1], pData[2]);

    } break;

    default: {
      half = count / 2;

      Sort(start, half);
      Sort(start + half, count - half);

      Merge(start, half, start + half, count - half);
    }
    }
  }

  bool RunTask_Sort(WorkerThread *pThread, int start, int count) {
    TaskCompletion Completion;
    SorterInternalTask Task(&Completion);
    int half;

    if (count < MinItemCountPerTask) {
      Sort(start, count);
      return true;
    }

    half = count / 2;

    /* sort each half */
    Task.Init(m_pSorter, start, half, TaskType_Sort);
    pThread->push_task(&Task);

    RunTask_Sort(pThread, start + half, count - half);

    pThread->work_until_done(&Completion);

    /* merge result */
    Merge(start, half, start + half, count - half);

    return true;
  }

  virtual bool Run(WorkerThread *pThread) {
    bool bOk;

    switch (m_TaskType) {
    case TaskType_Sort: {
      bOk = RunTask_Sort(pThread, m_Start, m_Count);
      assert(bOk);
    } break;

    default:
      assert(false);
    }

    /* DON'T delete this;  : these tasks get created on stack */
    return true;
  }

public:
  Sorter<T, Comparator, MinItemCountPerTask> *m_pSorter;
  eTaskType m_TaskType;
  int m_Start;
  int m_Count;
  int m_MergeRet;
};

template <class T, class Comparator, int MinItemCountPerTask>
void Sorter<T, Comparator, MinItemCountPerTask>::Sort() {
  TaskCompletion completion;

  SorterInternalTask<T, Comparator, MinItemCountPerTask> Task(&completion);

  WorkerThread *pThread = WorkerThread::current();

  Task.Init(this, 0, m_Count);

  pThread->push_task(&Task);
  pThread->work_until_done(&completion);
}

template <class T, class Comparator, int MinItemCountPerTask>
void Sorter<T, Comparator, MinItemCountPerTask>::Sort_Serial() {
  SorterInternalTask<T, Comparator, MinItemCountPerTask> Task(NULL);

  Task.Init(this, 0, m_Count);
  Task.Sort(0, m_Count);
}
}
