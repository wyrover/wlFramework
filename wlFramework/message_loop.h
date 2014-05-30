#ifndef WL_MESSAGE_LOOP_H_
#define WL_MESSAGE_LOOP_H_

#include <string>
#include <queue>
#include "stdafx.h"
#include "lock.h"
#include "scoped_ptr.h"
#include "ref_counted.h"
#include "weak_ptr.h"
#include "callback.h"

namespace wl {

class WL_EXPORT Location {
public:
  Location(const char* function_name, const char* file_name, int line_number, const void* program_counter);
  bool operator<(const Location& other) const;
  std::string ToString() const;
private:
  const char* function_name_;
  const char* file_name_;
  int line_number_;
  const void* program_counter_;
};

struct WL_EXPORT PendingTask {
  PendingTask(const Location& from, const Closure& task);
  PendingTask(const Location& from, const Closure& task, TimeTicks delayed_run_time, bool nestable);
  ~PendingTask();

  bool operator<(const PendingTask& other) const;
  Location posted_from;
  Closure task;
  int sequence_num;
  // the time when the task should be run.
  TimeTicks delayed_run_time;
  bool nestable;
};

class WL_EXPORT TaskQueue : public std::queue<PendingTask> {
public:
  void Swap(TaskQueue* queue);
};

typedef std::priority_queue<PendingTask> DelayedTaskQueue;

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
  struct RunState {
    Delegate* delegate;
    bool should_quit;
    int run_depth;
  };
private:
  static const int kMessageFilterCode = 0x5001;
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
  bool AddToIncomingQueue(const Location& from_here, const Closure& task, TimeDelta delay, bool nestable);
  void ReloadWorkQueue(TaskQueue* work_queue);
  void WillDestroyCurrentMessageLoop();
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

class WL_EXPORT RunLoop {
public:
  RunLoop();
  ~RunLoop();
  void Run();
  void RunUntilIdle();
  bool running() const { return running_; }
  void Quit();
  Closure QuitClosure();
  bool BeforeRun();
  void AfterRun();
private:
  friend class MessageLoop;
  MessageLoop* loop_;
  WeakPtrFactory<RunLoop> weak_factory_;
  RunLoop* previous_run_loop_;
  int run_depth_;
  bool run_called_;
  bool quit_called_;
  bool running_;
  bool quit_when_idle_received_;
  DISALLOW_COPY_AND_ASSIGN(RunLoop);
};

class WL_EXPORT MessageLoop : public MessagePump::Delegate {
public:
  enum Type {TYPE_DEFAULT, TYPE_UI, TYPE_IO};

  explicit MessageLoop(Type type);
  virtual ~MessageLoop();

  static MessageLoop* current();

  virtual bool DoWork() override;
  virtual bool DoDelayedWork(TimeTicks* next_delayed_work_time) override;
  virtual bool DoIdleWork() override;

  void PostTask(const Location& from_here, const Closure& task);
  void PostDelayedTask(const Location& from_here, const Closure& task, TimeDelta delay);
  void PostNonNestableTask(const Location& from_here, const Closure& task);
  void PostNonNestableDelayedTask(const Location& from_here, const Closure& task, TimeDelta delay);

  void Run();
  void RunUntilIdle();
  void Quit() { QuitWhenIdle(); }
  void QuitWhenIdle();
  void QuitNow();
  static Closure QuitClosure() { return QuitWhenIdleClosure(); }
  static Closure QuitWhenIdleClosure();
  Type type() const { return type_; }
  void SetNestableTasksAllowed(bool allowed);
  bool NestableTasksAllowed() const;
  bool IsNested();
  void set_os_modal_loop(bool os_modal_loop);
  bool os_modal_loop() const;
  bool is_running() const;
  MessagePump* pump() { return pump_.get(); }
private:
  friend class RunLoop;
  friend IncomingTaskQueue;
  void RunInternal();
  bool ProcessNextDelayedNonNestableTask();
  void RunTask(const PendingTask& pending_task);
  void AddToDelayedWorkQueue(const PendingTask& pending_task);
  bool DeferOrRunPendingTask(const PendingTask& pending_task);
  bool DeletePendingTasks();
  void ReloadWorkQueue();
  void ScheduleWork(bool was_empty);
  scoped_refptr<IncomingTaskQueue> incoming_task_queue_;
  scoped_ptr<MessagePump> pump_;
  bool os_modal_loop_;
  Type type_;
  TaskQueue work_queue_;
  DelayedTaskQueue delayed_work_queue_;
  TimeTicks recent_time_;
  TaskQueue deferred_non_nestable_work_queue_;
  bool nestable_tasks_allowed_;
  RunLoop* run_loop_;
  DISALLOW_COPY_AND_ASSIGN(MessageLoop);
};
}
extern "C" {
  void* _ReturnAddress();
};
#define FROM_HERE wl::Location(__FUNCTION__, __FILE__, __LINE__, _ReturnAddress())

#endif