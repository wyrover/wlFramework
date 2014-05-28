#include "stdafx.h"

#include "pending_task.h"

namespace wl {
PendingTask::PendingTask(const Location& from, const Closure& task)
  : posted_from(from)
  , task(task)
  , sequence_num(0)
  , delayed_run_time(0) {}

PendingTask::PendingTask(const Location& from, const Closure& task,
  TimeTicks delayed_run_time)
  : posted_from(from)
  , task(task)
  , sequence_num(0)
  , delayed_run_time(delayed_run_time) {}

PendingTask::~PendingTask() {}

bool PendingTask::operator<(const PendingTask& other) const {
  if (delayed_run_time != other.delayed_run_time)
    return delayed_run_time < other.delayed_run_time;
  return sequence_num - other.sequence_num > 0;
}

void TaskQueue::Swap(TaskQueue* queue) {
  c.swap(queue->c);
}
}