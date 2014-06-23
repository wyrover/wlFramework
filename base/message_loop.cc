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
  MessageLoop* g_loops[MessageLoop::Type_COUNT];

  struct ThreadParams {
    MessageLoop::Type type;
  };

  DWORD CALLBACK ThreadMain(void* params) {
    ThreadParams* thread_params = static_cast<ThreadParams*>(params);
    MessageLoop::Type type = thread_params->type;
    delete thread_params;
    scoped_ptr<MessageLoop> message_loop(new MessageLoop(type));
    {
      base::AutoLock locked(g_loops_lock);
      g_loops[type] = message_loop.get();
    }
    message_loop->Run();
    return 0;
  }
  base::ThreadLocalPointer<MessageLoop> g_tls;
}

MessageLoop* MessageLoop::current() {
  return g_tls.Get();
}

void MessageLoop::Start(Type type) {
  if (type > Type_UI && type < Type_COUNT) {
    ThreadParams* params = new ThreadParams();
    params->type = type;
    HANDLE thread_handle = CreateThread(NULL, 0, ThreadMain, (void*)params, 0, NULL);
    if (thread_handle) {
      CloseHandle(thread_handle);
    } else {
      delete params;
    }
  }
}

void MessageLoop::PostTask(Type type, const base::Closure& task) {
  bool target_thread_outlives_current =
    MessageLoop::current()->type() >= type;
  if (!target_thread_outlives_current)
    g_loops_lock.Acquire();
  MessageLoop* message_loop = g_loops[type];
  if (message_loop) {
    message_loop->PostTask(task);
  }
}

void MessageLoop::QuitCurrent() {
  MessageLoop::current()->Quit();
}

MessageLoop::MessageLoop(Type type)
  : atom_(0), type_(type) {
  InitMessageWnd();
  g_tls.Set(this);
}

MessageLoop::~MessageLoop() {
  DestroyWindow(message_hwnd_);
  UnregisterClass(MAKEINTATOM(atom_),
    GetModuleFromAddress(&WndProcThunk));
  g_tls.Set(NULL);
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
  ::PostQuitMessage(0);
}

void MessageLoop::PostTask(const base::Closure& task) {
  {
    base::AutoLock locked(tasks_lock_);
    tasks_.push(task);
  }
  ::PostMessage(message_hwnd_, kMsgHaveWork, reinterpret_cast<WPARAM>(this), 0);
}

void MessageLoop::DoWork() {
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

LRESULT CALLBACK MessageLoop::WndProcThunk(HWND window_handle,
  UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
  case kMsgHaveWork:
    reinterpret_cast<MessageLoop*>(wparam)->DoWork();
    break;
  }
  return ::DefWindowProc(window_handle, message, wparam, lparam);
}