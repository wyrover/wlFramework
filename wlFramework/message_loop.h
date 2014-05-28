#ifndef WL_MESSAGE_LOOP_H_
#define WL_MESSAGE_LOOP_H_

#include "pending_task.h"
#include "lock.h"
#include "scoped_ptr.h"
#include "ref_counted.h"

namespace wl {

class WL_EXPORT MessagePump {
public:
  class WL_EXPORT Delegate {
  public:
    virtual ~Delegate() {}

    virtual bool DoWork() = 0;

    virtual bool DoDelayedWork(TimeTicks* next_delayed_work_time) = 0;

    virtual bool DoIdleWork() = 0;
  };

  MessagePump();
  virtual ~MessagePump();

  virtual void Run(Delegate* delegate) = 0;

  virtual void Quit() = 0;

  virtual void ScheduleWork() = 0;

  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time) = 0;
};

class WL_EXPORT MessagePumpDefault : public MessagePump {
public:
  MessagePumpDefault();
  ~MessagePumpDefault();

  virtual void Run(Delegate* delegate) override;

  virtual void Quit() override;

  virtual void ScheduleWork() override;

  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override;

private:
  bool keep_running_;

  HANDLE handle_;

  TimeTicks delayed_work_time_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpDefault);
};

class WL_EXPORT MessagePumpUI : public MessagePump {
public:
  MessagePumpUI();
  virtual ~MessagePumpUI();

  virtual void Run(Delegate* delegate) override;

  virtual void Quit() override;

  virtual void ScheduleWork() override;

  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override;

private:
  struct RunState {
    Delegate* delegate;
    bool should_quit;
    int run_depth;
  };
  static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  void InitMessageWnd();
  int GetCurrentDelay() const;
  bool ProcessNextWindowsMessage();
  bool ProcessMessageHelper(const MSG& msg);
  bool ProcessPumpReplacementMessage();
  void WaitForWork();
  void HandleWorkMessage();
  void HandleTimerMessage();

  RunState* state_;

  ATOM atom_;

  HWND message_hwnd_;

  LONG have_work_;

  TimeTicks delayed_work_time_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpUI);
};

class WL_EXPORT MessagePumpIO : public MessagePump {
public:
  MessagePumpIO();
  virtual ~MessagePumpIO();

  virtual void Run(Delegate* delegate) override;

  virtual void Quit() override;

  virtual void ScheduleWork() override;

  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override;

private:

  DISALLOW_COPY_AND_ASSIGN(MessagePumpIO);
};

class MessageLoop;
class WL_EXPORT IncomingTaskQueue
  : public RefCountedThreadSafe<IncomingTaskQueue> {
public:
  explicit IncomingTaskQueue(MessageLoop* message_loop);
  bool AddToIncomingQueue(const Location& from_here, const Closure& task, TimeDelta delay);
  void ReloadWorkQueue(TaskQueue* work_queue);
private:
  friend class RefCountedThreadSafe<IncomingTaskQueue>;
  virtual ~IncomingTaskQueue();
  TimeTicks CalculateDelayedRunTime(TimeDelta delay);

  TaskQueue incoming_queue_;
  Lock incoming_queue_lock_;
  MessageLoop* message_loop_;
  int next_sequence_num_;
  DISALLOW_COPY_AND_ASSIGN(IncomingTaskQueue);
};

class WL_EXPORT MessageLoop : public MessagePump::Delegate {
public:
  enum Type {
    TYPE_DEFAULT,
    TYPE_UI,
    TYPE_IO
  };

  explicit MessageLoop(Type type);
  virtual ~MessageLoop();

  static MessageLoop* current();

  virtual bool DoWork() override;
  virtual bool DoDelayedWork(TimeTicks* next_delayed_work_time) override;
  virtual bool DoIdleWork() override;

  void PostTask(const Location& from_here, const Closure& task);
  void PostDelayedTask(const Location& from_here, const Closure& task, TimeDelta delay);
  void Run();
  Type type() const { return type_; }
  void set_os_modal_loop(bool os_modal_loop) { os_modal_loop_ = os_modal_loop; }
  bool os_modal_loop() const { return os_modal_loop_; }
  MessagePump* pump() { return pump_.get(); }
private:
  void AddToDelayedWorkQueue(const PendingTask& pending_task);
  bool DeferOrRunPendingTask(const PendingTask& pending_task);
  void RunTask(const PendingTask& pending_task);
  scoped_refptr<IncomingTaskQueue> incoming_task_queue_;
  scoped_ptr<MessagePump> pump_;
  bool os_modal_loop_;
  Type type_;
  TaskQueue work_queue_;
  DelayedTaskQueue delayed_work_queue_;
  TimeTicks recent_time_;
  DISALLOW_COPY_AND_ASSIGN(MessageLoop);
};
}

#endif