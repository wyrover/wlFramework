#ifndef WL_PENDING_TASK_H_
#define WL_PENDING_TASK_H_

#include <queue>

#include "stdafx.h"
#include "location.h"
#include "callback.h"

namespace wl {

struct WL_EXPORT PendingTask {
  PendingTask(const Location& from, const Closure& task);
  PendingTask(const Location& from, const Closure& task, unsigned long delay_ticks);
  ~PendingTask();

  bool operator<(const PendingTask& other) const;
  Location posted_from;
  Closure task;
  unsigned long time_posted;
  unsigned long delay_ticks;
};

class WL_EXPORT TaskQueue : public std::queue<PendingTask> {
public:
  void Swap(TaskQueue* queue);
};

typedef std::priority_queue<PendingTask> DelayedTaskQueue;
}

#endif