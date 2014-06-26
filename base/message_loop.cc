#include "message_loop.h"

#include <strsafe.h>
#include "base/thread_local.h"

namespace {
  static const wchar_t kWndClassFormat[] = L"WorkThreadWindow_%p";

  static const int kMsgHaveWork = WM_USER + 1;

  HMODULE GetModuleFromAddress(void* address) {
    HMODULE instance = NULL;
    if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
      GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      static_cast<char*>(address),
      &instance)) {
    }
    return instance;
  }
  base::Lock g_loops_lock;
  MessageLoop* g_loops[MessageLoop::ID_COUNT];
  HANDLE g_thread_handles[MessageLoop::ID_COUNT];

  struct ThreadParams {
    MessageLoop::ID id;
  };

  struct TimerParams {
    MessageLoop* message_loop;
    TimeTicks timer_id;
  };

  DWORD CALLBACK ThreadMain(void* params) {
    ThreadParams* thread_params = static_cast<ThreadParams*>(params);
    MessageLoop::ID type = thread_params->id;
    delete thread_params;
    scoped_ptr<MessageLoop> message_loop(new MessageLoop(type));
    message_loop->Run();
    return 0;
  }

  LRESULT CALLBACK WndProcThunk(HWND window_handle, UINT message, WPARAM wparam,
    LPARAM lparam) {
    switch (message) {
    case kMsgHaveWork:
      reinterpret_cast<MessageLoop*>(wparam)->HandleHaveWorkMessage();
      break;
    case WM_TIMER:
      {
        KillTimer(window_handle, wparam);
        TimerParams* params = reinterpret_cast<TimerParams*>(wparam);
        params->message_loop->HandleTimerMessage(params->timer_id);
        delete params;
        break;
      }
    }
    return ::DefWindowProc(window_handle, message, wparam, lparam);
  }

  void QuitCurrentHelper() {
    MessageLoop::current()->Quit();
  }

  base::ThreadLocalPointer<MessageLoop> g_tls;
}

MessageLoop* MessageLoop::current() {
  return g_tls.Get();
}

void MessageLoop::Start(ID identifier) {
  if (identifier > UI && identifier < ID_COUNT) {
    ThreadParams* params = new ThreadParams();
    params->id = identifier;
    g_thread_handles[identifier] = CreateThread(NULL, 0, ThreadMain, (void*)params, 0, NULL);
    if (!g_thread_handles[identifier]) {
      delete params;
    }
  }
}

void MessageLoop::Stop(ID identifier) {
  if (identifier > UI && identifier < ID_COUNT) {
    PostTask(identifier, base::Bind(&QuitCurrentHelper));
    DWORD result = WaitForSingleObject(g_thread_handles[identifier], INFINITE);
    CloseHandle(g_thread_handles[identifier]);
    g_thread_handles[identifier] = 0;
  }
}

void MessageLoop::PostTask(ID identifier, const base::Closure& task) {
  PostDelayedTask(identifier, task, 0);
}

void MessageLoop::PostDelayedTask(ID identifier, const base::Closure& task, TimeDelta delayed_ms) {
  bool target_thread_outlives_current =
    MessageLoop::current()->id() >= identifier;
  if (!target_thread_outlives_current)
    g_loops_lock.Acquire();
  MessageLoop* message_loop = g_loops[identifier];
  if (message_loop) {
    message_loop->PostandSchduleTask(task, delayed_ms);
  }
  if (!target_thread_outlives_current)
    g_loops_lock.Release();
}

bool MessageLoop::CurrentlyOn(ID identifer) {
  return current()->id() == identifer;
}

MessageLoop::MessageLoop(ID identifier)
  : atom_(0), id_(identifier) {
  InitMessageWnd();
  g_tls.Set(this);
  base::AutoLock locked(g_loops_lock);
  g_loops[identifier] = this;
}

MessageLoop::~MessageLoop() {
  DestroyWindow(message_hwnd_);
  UnregisterClass(MAKEINTATOM(atom_),
    GetModuleFromAddress(&WndProcThunk));
  g_tls.Set(NULL);
  base::AutoLock locked(g_loops_lock);
  g_loops[id_] = NULL;
}

void MessageLoop::InitMessageWnd() {
  wchar_t class_name[MAX_PATH] = {0};
  StringCchPrintf(class_name, MAX_PATH-1, kWndClassFormat, this);
  HINSTANCE instance = GetModuleFromAddress(&WndProcThunk);
  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = &WndProcThunk;
  wc.hInstance = instance;
  wc.lpszClassName = class_name;
  atom_ = RegisterClassEx(&wc);

  message_hwnd_ = CreateWindow(MAKEINTATOM(atom_), 0, 0, 0, 0, 0, 0,
    HWND_MESSAGE, 0, instance, 0);
}

void MessageLoop::Run() {
  BOOL bRet = FALSE;
  MSG msg;
  while((bRet = ::GetMessage(&msg, NULL, 0, 0)) != 0) {
    if (bRet == -1) {
      continue;
    } else {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
}

void MessageLoop::Quit() {
  PostQuitMessage(0);
}

void MessageLoop::PostandSchduleTask(const base::Closure& task, TimeDelta delayed_ms) {
  base::AutoLock locked(tasks_lock_);
  if (delayed_ms == 0) {
    tasks_.push(task);
    ::PostMessage(message_hwnd_, kMsgHaveWork, reinterpret_cast<WPARAM>(this), 0);
  } else {
    TimerParams* params = new TimerParams();
    params->message_loop = this;
    params->timer_id = ::GetTickCount();
    delayed_tasks_.insert(std::make_pair(params->timer_id, task));
    ::SetTimer(message_hwnd_, reinterpret_cast<UINT_PTR>(params), delayed_ms, NULL);
  }
}

void MessageLoop::HandleHaveWorkMessage() {
  if (work_queue_.empty()) {
    base::AutoLock locked(tasks_lock_);
    if (!tasks_.empty())
      tasks_.swap(work_queue_);
  }

  if (!work_queue_.empty()) {
    base::Closure task = work_queue_.front();
    work_queue_.pop();
    task.Run();
  }
}

void MessageLoop::HandleTimerMessage(TimeTicks timer_id) {
  base::AutoLock locked(tasks_lock_);
  std::map<TimeTicks, base::Closure>::iterator iter = delayed_tasks_.find(timer_id);
  if (iter != delayed_tasks_.end()) {
    base::Closure task = iter->second;
    delayed_tasks_.erase(iter);
    task.Run();
  }
}