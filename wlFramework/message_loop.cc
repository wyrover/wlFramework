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
  : atom_(0), state_(NULL) {
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
    more_work_is_plausible |= delegate->DoWork();
    if (state_->should_quit)
      break;
    more_work_is_plausible |= delegate->DoDelayedWork(&delayed_work_time_);

    if (more_work_is_plausible && delayed_work_time_ == 0)
      KillTimer(message_hwnd_, reinterpret_cast<UINT_PTR>(this));
    if (state_->should_quit)
      break;
    if (more_work_is_plausible)
      continue;

    more_work_is_plausible = delegate->DoIdleWork();
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

  BOOL ret = PostMessage(message_hwnd_, kMsgHaveWork, reinterpret_cast<WPARAM>(this), 0);
  if (ret)
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

  TranslateMessage(&msg);
  DispatchMessage(&msg);
  return true;
}

bool MessagePumpUI::ProcessPumpReplacementMessage() {
  bool have_message = false;
  MSG msg;
  if (MessageLoop::current()->os_modal_loop()) {
    have_message = PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_REMOVE)
      || PeekMessage(&msg, NULL, WM_TIMER, WM_TIMER, PM_REMOVE);
  } else {
    have_message = !!PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
  }

  InterlockedExchange(&have_work_, 0);
  if (!have_message)
    return false;

  ScheduleWork();
  return ProcessMessageHelper(msg);
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
  if (!state_)
    return;

  state_->delegate->DoDelayedWork(&delayed_work_time_);
  if (delayed_work_time_ != 0) {
    ScheduleDelayedWork(delayed_work_time_);
  }
}

IncomingTaskQueue::IncomingTaskQueue(MessageLoop* message_loop)
  : message_loop_(message_loop), next_sequence_num_(0) {}

bool IncomingTaskQueue::AddToIncomingQueue(const Location& from_here,
  const Closure& task, TimeDelta delay) {
  AutoLock locked(incoming_queue_lock_);
  PendingTask pending_task(from_here, task, CalculateDelayedRunTime(delay));
  pending_task.sequence_num = next_sequence_num_++;
  bool was_empty = incoming_queue_.empty();
  incoming_queue_.push(pending_task);

  if (was_empty)
    message_loop_->pump()->ScheduleWork();
  return true;
}

void IncomingTaskQueue::ReloadWorkQueue(TaskQueue* work_queue) {
  AutoLock locked(incoming_queue_lock_);
  if (!incoming_queue_.empty())
    incoming_queue_.Swap(work_queue);
}

TimeTicks IncomingTaskQueue::CalculateDelayedRunTime(TimeDelta delay) {
  TimeTicks delayed_run_time = 0;
  if (delay > 0) {
    delayed_run_time = ::GetTickCount() + delay;
  }
  return delayed_run_time;
}

MessageLoop::MessageLoop(Type type) : type_(type) {
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

}

// static
MessageLoop* MessageLoop::current() {
  return tls_ptr.Get();
}

bool MessageLoop::DoWork() {
  for (;;) {
    if (work_queue_.empty())
      incoming_task_queue_->ReloadWorkQueue(&work_queue_);
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
  if (delayed_work_queue_.empty()) {
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
  return false;
}

void MessageLoop::PostTask(const Location& from_here, const Closure& task) {
  incoming_task_queue_->AddToIncomingQueue(from_here, task, 0);
}

void MessageLoop::PostDelayedTask(const Location& from_here, const Closure& task, TimeDelta delay) {
  incoming_task_queue_->AddToIncomingQueue(from_here, task, delay);
}

void MessageLoop::Run() {
  pump_->Run(this);
}

void MessageLoop::AddToDelayedWorkQueue(const PendingTask& pending_task) {
  delayed_work_queue_.push(pending_task);
}

bool MessageLoop::DeferOrRunPendingTask(const PendingTask& pending_task) {
  RunTask(pending_task);
  return true;
}

void MessageLoop::RunTask(const PendingTask& pending_task) {
  pending_task.task.Run();
}

}