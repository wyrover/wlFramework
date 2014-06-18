#include "workthread.h"

#include <strsafe.h>

namespace {
  static const wchar_t kWndClassFormat[] = L"WorkThreadWindow_%p";

  HMODULE GetModuleFromAddress(void* address) {
    HMODULE instance = NULL;
    if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
      GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      static_cast<char*>(address),
      &instance)) {
    }
    return instance;
  }
}

WorkThread::WorkThread()
  : atom_(0) {
}

WorkThread::~WorkThread() {
  DestroyWindow(message_hwnd_);
  UnregisterClass(MAKEINTATOM(atom_),
    GetModuleFromAddress(&WndProcThunk));
}

void WorkThread::InitMessageWnd() {
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

LRESULT CALLBACK WorkThread::WndProcThunk(HWND window_handle,
  UINT message, WPARAM wparam, LPARAM lparam) {
  return ::DefWindowProc(window_handle, message, wparam, lparam);
}