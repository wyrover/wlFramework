#ifndef WL_MESSAGE_LOOP_H_
#define WL_MESSAGE_LOOP_H_

#include "pending_task.h"
#include "lock.h"

namespace wl {
class WL_EXPORT MessageLoop {
public:
  enum Type {
    TYPE_DEFAULT,
    TYPE_UI,
    TYPE_IO
  };

  explicit MessageLoop(Type type = TYPE_DEFAULT);
  virtual ~MessageLoop();

  static MessageLoop* current();

  void PostTask(const Location& from_here, const Closure& task);
  void PostDelayedTask(const Location& from_here, const Closure& task, unsigned long delay);
  void Run();
  Type type() const { return type_; }
private:
  void RunTask(const PendingTask& pending_task);
  bool DeferOrRunPendingTask(const PendingTask& pending_task);
  void AddToDelayedWorkQueue(const PendingTask& pending_task);
  bool DeletePendingTasks();
  void ReloadWorkQueue();
  void ScheduleWork(bool was_empty);
  TaskQueue incoming_queue_;
  Lock incoming_queue_lock_;

  Type type_;
  TaskQueue work_queue_;
  DelayedTaskQueue delayed_work_queue_;
  DISALLOW_COPY_AND_ASSIGN(MessageLoop);
};

class WL_EXPORT MessageLoopForUI : public MessageLoop {
public:
  MessageLoopForUI();
  virtual ~MessageLoopForUI();
  static MessageLoopForUI* current() {
    return static_cast<MessageLoopForUI*>(MessageLoop::current());
  }
private:
  void InitMessageWnd();
  static LRESULT CALLBACK WndProcThunk(HWND window_handle, UINT message, WPARAM wparam, LPARAM lparam);
  ATOM atom_;
  HWND message_hwnd_;
};

class WL_EXPORT MessageLoopForIO : public MessageLoop {
public:
  MessageLoopForIO() : MessageLoop(TYPE_IO) {}
  static MessageLoopForIO* current() {
    return static_cast<MessageLoopForIO*>(MessageLoop::current());
  }
}
}

#endif