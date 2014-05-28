#include "stdafx.h"

#include "message_loop.h"

static const wchar_t kWndClassFormat[] = L"WL_MessageLoopWindow_%p"
namespace wl {
MessageLoop::MessageLoop(Type type) : type_(type) {

}

MessageLoopForUI::MessageLoopForUI() : MessageLoop(TYPE_UI) {
  InitMessageWnd();
}

MessageLoopForUI::~MessageLoopForUI() {
  DestroyWindow(message_hwnd_);
  HINSTANCE instance = NULL;
  ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<char*>(&WndProcThunk), &instance);
  UnregisterClass(MAKEINTATOM(atom_), instance);
}

void MessageLoopForUI::InitMessageWnd() {
  wchar_t class_name[100] = {0};
  swprintf_s(class_name, kWndClassFormat, this);
  HINSTANCE instance = NULL;
  ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<char*>(&WndProcThunk), &instance);
  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProcThunk;
  wc.lpszClassName = class_name;
  atom_ = RegisterClassEx(&wc);
  message_hwnd_ = CreateWindow(MAKEINTATOM(atom_), 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0, instance, 0);
}
}