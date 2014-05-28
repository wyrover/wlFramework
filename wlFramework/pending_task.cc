#include "stdafx.h"

#include "pending_task.h"

namespace wl {
PendingTask::PendingTask(const Location& from, const Closure& task)
  : posted_from(from)
  , task(task)
  , delay_ticks(0)
  , time_posted(::GetTickCount()) {}

PendingTask::PendingTask(const Location& from, const Closure& task,
  unsigned long delay_ticks)
  : posted_from(from)
  , task(task)
  , delay_ticks(delay_ticks)
  , time_posted(::GetTickCount()) {}

PendingTask::~PendingTask() {}

bool PendingTask::operator<(const PendingTask& other) const {
  if (delay_ticks != other.delay_ticks)
    return delay_ticks < other.delay_ticks;
  return time_posted < other.time_posted;
}

void TaskQueue::Swap(TaskQueue* queue) {
  c.swap(queue->c);
}
}