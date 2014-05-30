#include "stdafx.h"

#include "message_loop.h"
#include "thread_local.h"

namespace {
static const wchar_t kWndClassFormat[] = L"WL_MessageLoopWindow_%p";

static const int kMsgHaveWork = WM_USER + 1;

HMODULE GetModuleFromAddress(void* address) {
  HMODULE instance = NULL;
  ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<char*>(address), &instance);
  return instance;
}

wl::ThreadLocalPointer<wl::MessageLoop> tls_ptr;
}

namespace wl {

Lock::Lock() {
  ::InitializeCriticalSectionAndSpinCount(&cs_, 2000);
}
Lock::~Lock() {
  ::DeleteCriticalSection(&cs_);
}

Location::Location(const char* function_name, const char* file_name,
  int line_number, const void* program_counter)
  : function_name_(function_name)
  , file_name_(file_name)
  , line_number_(line_number)
  , program_counter_(program_counter) {}

bool Location::operator<(const Location& other) const {
  if (line_number_ != other.line_number_)
    return line_number_ < other.line_number_;
  if (file_name_ != other.file_name_)
    return file_name_ < other.file_name_;
  return function_name_ < other.function_name_;
}

std::string Location::ToString() const {
  return std::string(function_name_) + "@" + file_name_ + ":"
    + std::to_string((long long)line_number_);
}

PendingTask::PendingTask(const Location& from, const Closure& task)
  : posted_from(from)
  , task(task)
  , sequence_num(0)
  , delayed_run_time(0)
  , nestable(true) {}

PendingTask::PendingTask(const Location& from, const Closure& task,
  TimeTicks delayed_run_time, bool nestable)
  : posted_from(from)
  , task(task)
  , sequence_num(0)
  , delayed_run_time(delayed_run_time)
  , nestable(nestable) {}

PendingTask::~PendingTask() {}

bool PendingTask::operator<(const PendingTask& other) const {
  if (delayed_run_time != other.delayed_run_time)
    return delayed_run_time < other.delayed_run_time;
  return sequence_num - other.sequence_num > 0;
}

void TaskQueue::Swap(TaskQueue* queue) {
  c.swap(queue->c);
}

MessagePump::MessagePump() {}

MessagePump::~MessagePump() {}

MessagePumpDefault::MessagePumpDefault()
  : keep_running_(true) {
  handle_ = CreateEvent(NULL, false, false, NULL);
}

MessagePumpDefault::~MessagePumpDefault() {
  CloseHandle(handle_);
}

void MessagePumpDefault::Run(Delegate* delegate) {
  for (;;) {
    bool did_work = delegate->DoWork();
    if (!keep_running_)
      break;
    did_work |= delegate->DoDelayedWork(&delayed_work_time_);
    if (!keep_running_)
      break;
    if (did_work)
      continue;
    did_work = delegate->DoIdleWork();
    if (!keep_running_)
      break;
    if (did_work)
      continue;
    if (delayed_work_time_ == 0) {
      WaitForSingleObject(handle_, INFINITE);
    } else {
      TimeDelta delay = delayed_work_time_ - ::GetTickCount();
      if (delay > 0) {
        WaitForSingleObject(handle_, static_cast<DWORD>(delay));
      } else {
        delayed_work_time_ = 0;
      }
    }
  }
  keep_running_ = true;
}

void MessagePumpDefault::Quit() {
  keep_running_ = false;
}

void MessagePumpDefault::ScheduleWork() {
  SetEvent(handle_);
}

void MessagePumpDefault::ScheduleDelayedWork(const TimeTicks& delayed_work_time) {
  delayed_work_time_ = delayed_work_time;
}

MessagePumpUI::MessagePumpUI()
  : atom_(0) {
  InitMessageWnd();
}

MessagePumpUI::~MessagePumpUI() {
  DestroyWindow(message_hwnd_);
  UnregisterClass(MAKEINTATOM(atom_), GetModuleFromAddress(&WndProcThunk));
}

void MessagePumpUI::Run(Delegate* delegate) {
  RunState s;
  s.delegate = delegate;
  s.should_quit = false;
  s.run_depth = state_ ? state_->run_depth + 1 : 1;
  RunState* previous_state = state_;
  state_ = &s;

  for (;;) {
    bool more_work_is_plausible = ProcessNextWindowsMessage();
    if (state_->should_quit)
      break;
    more_work_is_plausible |= state_->delegate->DoWork();
    if (state_->should_quit)
      break;
    more_work_is_plausible |= state_->delegate->DoDelayedWork(&delayed_work_time_);

    if (more_work_is_plausible && delayed_work_time_ == 0)
      KillTimer(message_hwnd_, reinterpret_cast<UINT_PTR>(this));
    if (state_->should_quit)
      break;
    if (more_work_is_plausible)
      continue;

    more_work_is_plausible = state_->delegate->DoIdleWork();
    if (state_->should_quit)
      break;
    if (more_work_is_plausible)
      continue;
    WaitForWork();
  }
  state_ = previous_state;
}

void MessagePumpUI::Quit() {
  state_->should_quit = true;
}

void MessagePumpUI::ScheduleWork() {
  if (InterlockedExchange(&have_work_, 1))
    return;

  if (PostMessage(message_hwnd_, kMsgHaveWork, reinterpret_cast<WPARAM>(this), 0))
    return;

  InterlockedExchange(&have_work_, 0);
}

void MessagePumpUI::ScheduleDelayedWork(const TimeTicks& delayed_work_time) {
  delayed_work_time_ = delayed_work_time;
  long delay = GetCurrentDelay();
  if (delay < USER_TIMER_MINIMUM)
    delay = USER_TIMER_MINIMUM;

  SetTimer(message_hwnd_, reinterpret_cast<UINT_PTR>(this), delay, NULL);
}

LRESULT CALLBACK MessagePumpUI::WndProcThunk(HWND hwnd,
  UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
  case kMsgHaveWork:
    reinterpret_cast<MessagePumpUI*>(wparam)->HandleWorkMessage();
    break;
  case WM_TIMER:
    reinterpret_cast<MessagePumpUI*>(wparam)->HandleTimerMessage();
    break;
  }
  return DefWindowProc(hwnd, message, wparam, lparam);
}

void MessagePumpUI::InitMessageWnd() {
  wchar_t class_name[100] = {0};
  swprintf_s(class_name, kWndClassFormat, this);
  HINSTANCE instance = GetModuleFromAddress(&WndProcThunk);
  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProcThunk;
  wc.lpszClassName = class_name;
  atom_ = RegisterClassEx(&wc);
  message_hwnd_ = CreateWindow(MAKEINTATOM(atom_), 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0, instance, 0);
}

int MessagePumpUI::GetCurrentDelay() const {
  if (delayed_work_time_ == 0)
    return -1;
  TimeDelta timeout = delayed_work_time_ - ::GetTickCount();
  int delay = static_cast<int>(timeout);
  if (delay < 0) delay = 0;
  return delay;
}

bool MessagePumpUI::ProcessNextWindowsMessage() {
  bool sent_messages_in_queue = false;
  DWORD queue_status = GetQueueStatus(QS_SENDMESSAGE);
  if (HIWORD(queue_status) & QS_SENDMESSAGE)
    sent_messages_in_queue = true;

  MSG msg;
  if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    return ProcessMessageHelper(msg);
  }

  return sent_messages_in_queue;
}

bool MessagePumpUI::ProcessMessageHelper(const MSG& msg) {
  if (WM_QUIT == msg.message) {
    state_->should_quit = true;
    PostQuitMessage(static_cast<int>(msg.wParam));
    return false;
  }

  if (msg.message == kMsgHaveWork && msg.hwnd == message_hwnd_)
    return ProcessPumpReplacementMessage();

  if (CallMsgFilter(const_cast<MSG*>(&msg), kMessageFilterCode))
    return true;

  TranslateMessage(&msg);
  DispatchMessage(&msg);
  return true;
}

void MessagePumpUI::WaitForWork() {
  long delay = GetCurrentDelay();
  if (delay < 0)
    delay = INFINITE;
  DWORD result;
  result = MsgWaitForMultipleObjectsEx(0, NULL, delay, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
  if (WAIT_OBJECT_0 == result) {
    MSG msg = {0};
    DWORD queue_status = GetQueueStatus(QS_MOUSE);
    if (HIWORD(queue_status) & QS_MOUSE
      && PeekMessage(&msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_NOREMOVE)) {
      WaitMessage();
    }
    return;
  }
}

void MessagePumpUI::HandleWorkMessage() {

  if (!state_) {
    InterlockedExchange(&have_work_, 0);
    return;
  }

  ProcessPumpReplacementMessage();

  if (state_->delegate->DoWork())
    ScheduleWork();
}

void MessagePumpUI::HandleTimerMessage() {
  KillTimer(message_hwnd_, reinterpret_cast<UINT_PTR>(this));
  if (!state_) return;
  state_->delegate->DoDelayedWork(&delayed_work_time_);
  if (delayed_work_time_ != 0) {
    ScheduleDelayedWork(delayed_work_time_);
  }
}

IncomingTaskQueue::IncomingTaskQueue(MessageLoop* message_loop)
  : message_loop_(message_loop), next_sequence_num_(0) {}

IncomingTaskQueue::~IncomingTaskQueue() {}

bool IncomingTaskQueue::AddToIncomingQueue(const Location& from_here,
  const Closure& task, TimeDelta delay, bool nestable) {
  AutoLock locked(incoming_queue_lock_);
  PendingTask pending_task(from_here, task, CalculateDelayedRunTime(delay), nestable);
  if (!message_loop_) {
    pending_task.task.Reset();
    return false;
  }

  pending_task.sequence_num = next_sequence_num_++;
  bool was_empty = incoming_queue_.empty();
  incoming_queue_.push(pending_task);
  pending_task.task.Reset();

  message_loop_->ScheduleWork(was_empty);
  return true;
}

void IncomingTaskQueue::ReloadWorkQueue(TaskQueue* work_queue) {
  AutoLock locked(incoming_queue_lock_);
  if (!incoming_queue_.empty())
    incoming_queue_.Swap(work_queue);
}

void IncomingTaskQueue::WillDestroyCurrentMessageLoop() {
  AutoLock lock(incoming_queue_lock_);
  message_loop_ = NULL;
}

TimeTicks IncomingTaskQueue::CalculateDelayedRunTime(TimeDelta delay) {
  TimeTicks delayed_run_time = 0;
  if (delay > 0) {
    delayed_run_time = ::GetTickCount() + delay;
  }
  return delayed_run_time;
}

RunLoop::RunLoop()
  : loop_(MessageLoop::current())
  , weak_factory_(this)
  , previous_run_loop_(NULL)
  , run_depth_(0)
  , run_called_(false)
  , quit_called_(false)
  , running_(false)
  , quit_when_idle_received_(false) {}

RunLoop::~RunLoop() {}

void RunLoop::Run() {
  if (!BeforeRun())
    return;
  loop_->RunInternal();
  AfterRun();
}

void RunLoop::RunUntilIdle() {
  quit_when_idle_received_ = true;
  Run();
}

void RunLoop::Quit() {
  quit_called_ = true;
  if (running_ && loop_->run_loop_ == this) {
    loop_->QuitNow();
  }
}

Closure RunLoop::QuitClosure() {
  return Bind(&RunLoop::Quit, weak_factory_.GetWeakPtr());
}

bool RunLoop::BeforeRun() {
  if (run_called_)
    return false;
  previous_run_loop_ = loop_->run_loop_;
  run_depth_ = previous_run_loop_ ? previous_run_loop_->run_depth_ + 1 : 1;
  loop_->run_loop_ = this;

  running_ = true;
  return true;
}

void RunLoop::AfterRun() {
  running_ = false;

  loop_->run_loop_ = previous_run_loop_;
  if (previous_run_loop_ && previous_run_loop_->quit_called_)
    loop_->QuitNow();
}

MessageLoop::MessageLoop(Type type)
  : type_(type)
  , nestable_tasks_allowed_(true)
  , os_modal_loop_(false)
  , run_loop_(NULL) {
  tls_ptr.Set(this);

  switch (type) {
  case TYPE_DEFAULT:
    pump_.reset(new MessagePumpDefault());
    break;
  case TYPE_UI:
    pump_.reset(new MessagePumpUI());
    break;
  case TYPE_IO:
    pump_.reset(new MessagePumpIO());
    break;
  default:
    break;
  }
  incoming_task_queue_ = new IncomingTaskQueue(this);
}

MessageLoop::~MessageLoop() {
  bool did_work;
  for (int i = 0; i < 100; ++i) {
    DeletePendingTasks();
    ReloadWorkQueue();
    did_work = DeletePendingTasks();
    if (!did_work)
      break;
  }

  incoming_task_queue_->WillDestroyCurrentMessageLoop();
  incoming_task_queue_ = NULL;
  tls_ptr.Set(NULL);
}

// static
MessageLoop* MessageLoop::current() {
  return tls_ptr.Get();
}

bool MessageLoop::DoWork() {
  if (!nestable_tasks_allowed_) {
    return false;
  }
  for (;;) {
    ReloadWorkQueue();
    if (work_queue_.empty())
      break;

    do {
      PendingTask pending_task = work_queue_.front();
      work_queue_.pop();
      if (pending_task.delayed_run_time != 0) {
        AddToDelayedWorkQueue(pending_task);
        if (delayed_work_queue_.top().task.Equals(pending_task.task))
          pump_->ScheduleDelayedWork(pending_task.delayed_run_time);
      } else {
        if (DeferOrRunPendingTask(pending_task))
          return true;
      }
    } while(!work_queue_.empty());
  }
  return false;
}

bool MessageLoop::DoDelayedWork(TimeTicks* next_delayed_work_time) {
  if (!nestable_tasks_allowed_ || delayed_work_queue_.empty()) {
    recent_time_ = *next_delayed_work_time = 0;
    return false;
  }

  TimeTicks next_run_time = delayed_work_queue_.top().delayed_run_time;
  if (next_run_time > recent_time_) {
    recent_time_ = ::GetTickCount();
    if (next_run_time > recent_time_) {
      *next_delayed_work_time = next_run_time;
      return false;
    }
  }

  PendingTask pending_task = delayed_work_queue_.top();
  delayed_work_queue_.pop();
  if (!delayed_work_queue_.empty())
    *next_delayed_work_time = delayed_work_queue_.top().delayed_run_time;

  return DeferOrRunPendingTask(pending_task);
}

bool MessageLoop::DoIdleWork() {
  if (ProcessNextDelayedNonNestableTask())
    return true;
  if (run_loop_->quit_when_idle_received_)
    pump_->Quit();

  return false;
}

void MessageLoop::PostTask(const Location& from_here, const Closure& task) {
  incoming_task_queue_->AddToIncomingQueue(from_here, task, 0, true);
}

void MessageLoop::PostDelayedTask(const Location& from_here, const Closure& task, TimeDelta delay) {
  incoming_task_queue_->AddToIncomingQueue(from_here, task, delay, true);
}

void MessageLoop::PostNonNestableTask(const Location& from_here, const Closure& task) {
  incoming_task_queue_->AddToIncomingQueue(from_here, task, 0, false);
}

void MessageLoop::PostNonNestableDelayedTask(const Location& from_here, const Closure& task, TimeDelta delay) {
  incoming_task_queue_->AddToIncomingQueue(from_here, task, delay, false);
}

void MessageLoop::Run() {
  RunLoop run_loop;
  run_loop.Run();
}

void MessageLoop::RunUntilIdle() {
  RunLoop run_loop;
  run_loop.RunUntilIdle();
}

void MessageLoop::QuitWhenIdle() {
  if (run_loop_) {
    run_loop_->quit_when_idle_received_ = true;
  }
}

void MessageLoop::QuitNow() {
  if (run_loop_) {
    pump_->Quit();
  }
}

static void QuitCurrentWhenIdle() {
  MessageLoop::current()->QuitWhenIdle();
}

Closure MessageLoop::QuitWhenIdleClosure() {
  return Bind(&QuitCurrentWhenIdle);
}

void MessageLoop::SetNestableTasksAllowed(bool allowed) {
  if (nestable_tasks_allowed_ != allowed) {
    nestable_tasks_allowed_ = allowed;
    if (!nestable_tasks_allowed_)
      return;

    pump_->ScheduleWork();
  }
}

bool MessageLoop::NestableTasksAllowed() const {
  return nestable_tasks_allowed_;
}

bool MessageLoop::IsNested() {
  return run_loop_->run_depth_ > 1;
}

bool MessageLoop::is_running() const {
  return run_loop_ != NULL;
}

void MessageLoop::set_os_modal_loop(bool os_modal_loop) {
  os_modal_loop_ = os_modal_loop;
}

bool MessageLoop::os_modal_loop() const {
  return os_modal_loop_;
}

void MessageLoop::RunInternal() {
  pump_->Run(this);
}

bool MessageLoop::ProcessNextDelayedNonNestableTask() {
  if (run_loop_->run_depth_ != 1)
    return false;
  if (deferred_non_nestable_work_queue_.empty())
    return false;
  PendingTask pending_task = deferred_non_nestable_work_queue_.front();
  deferred_non_nestable_work_queue_.pop();

  RunTask(pending_task);
  return true;
}

void MessageLoop::AddToDelayedWorkQueue(const PendingTask& pending_task) {
  delayed_work_queue_.push(pending_task);
}

bool MessageLoop::DeferOrRunPendingTask(const PendingTask& pending_task) {
  if (pending_task.nestable || run_loop_->run_depth_ == 1) {
    RunTask(pending_task);
    return true;
  }

  deferred_non_nestable_work_queue_.push(pending_task);
  return false;
}

bool MessageLoop::DeletePendingTasks() {
  bool did_work = !work_queue_.empty();
  while (!work_queue_.empty()) {
    PendingTask pending_task = work_queue_.front();
    work_queue_.pop();
    if (pending_task.delayed_run_time != 0) {
      AddToDelayedWorkQueue(pending_task);
    }
  }
  did_work |= !deferred_non_nestable_work_queue_.empty();
  while (!deferred_non_nestable_work_queue_.empty()) {
    deferred_non_nestable_work_queue_.pop();
  }

  did_work |= !delayed_work_queue_.empty();
  while (!delayed_work_queue_.empty()) {
    delayed_work_queue_.pop();
  }

  return did_work;
}

void MessageLoop::ReloadWorkQueue() {
  if (work_queue_.empty())
    incoming_task_queue_->ReloadWorkQueue(&work_queue_);
}

void MessageLoop::ScheduleWork(bool was_empty) {
  if (was_empty)
    pump_->ScheduleWork();
}

void MessageLoop::RunTask(const PendingTask& pending_task) {
  nestable_tasks_allowed_ = false;
  pending_task.task.Run();
  nestable_tasks_allowed_ = true;
}

}