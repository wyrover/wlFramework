#include "main_runner.h"

#include <time.h>

#include "base/message_loop.h"
#include "base/closure.h"
#include "resource.h"

namespace {
#define MAX_LOADSTRING 100

  // Global Variables:
  HINSTANCE hInst;								// current instance
  TCHAR szTitle[MAX_LOADSTRING] = L"HelloWorld";
  TCHAR szWindowClass[MAX_LOADSTRING] = L"HelloWorld";

  // Forward declarations of functions included in this code module:
  ATOM				MyRegisterClass(HINSTANCE hInstance);
  BOOL				InitInstance(HINSTANCE, int);
  LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
  INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

  ATOM MyRegisterClass(HINSTANCE hInstance)
  {
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= WndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= hInstance;
    wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WLFRAMEWORK));
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_WLFRAMEWORK);
    wcex.lpszClassName	= szWindowClass;
    wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcex);
  }

  //
  //   FUNCTION: InitInstance(HINSTANCE, int)
  //
  //   PURPOSE: Saves instance handle and creates main window
  //
  //   COMMENTS:
  //
  //        In this function, we save the instance handle in a global variable and
  //        create and display the main program window.
  //
  BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
  {
    HWND hWnd;

    hInst = hInstance; // Store instance handle in our global variable

    hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {
      return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
  }

  //
  //  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
  //
  //  PURPOSE:  Processes messages for the main window.
  //
  //  WM_COMMAND	- process the application menu
  //  WM_PAINT	- Paint the main window
  //  WM_DESTROY	- post a quit message and return
  //
  //
  void ShowAboutDialogOnUI(const HWND& hWnd) {
    DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
  }
  void MaybeShowAboutDialog(const HWND& hWnd) {
    srand((unsigned)time(NULL));
    if (true || rand() % 10 > 5) {
      MessageLoop::PostDelayedTask(MessageLoop::UI, base::Bind(ShowAboutDialogOnUI, hWnd), 5*1000);
    }
  }
  LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
  {
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)
    {
    case WM_COMMAND:
      wmId    = LOWORD(wParam);
      wmEvent = HIWORD(wParam);
      // Parse the menu selections:
      switch (wmId)
      {
      case IDM_ABOUT:
        MessageLoop::PostDelayedTask(MessageLoop::IO, base::Bind(&MaybeShowAboutDialog, hWnd), 5*1000);
        break;
      case IDM_EXIT:
        DestroyWindow(hWnd);
        break;
      default:
        return DefWindowProc(hWnd, message, wParam, lParam);
      }
      break;
    case WM_PAINT:
      hdc = BeginPaint(hWnd, &ps);
      // TODO: Add any drawing code here...
      EndPaint(hWnd, &ps);
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
  }

  // Message handler for about box.
  INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
  {
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
      return (INT_PTR)TRUE;

    case WM_COMMAND:
      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
      {
        EndDialog(hDlg, LOWORD(wParam));
        return (INT_PTR)TRUE;
      }
      break;
    }
    return (INT_PTR)FALSE;
  }
}

MainRunner* MainRunner::Create() {
  return new MainRunner();
}

MainRunner::MainRunner() {
}

MainRunner::~MainRunner() {}

void MainRunner::Initilize() {
  for (size_t id = MessageLoop::UI + 1; id < MessageLoop::ID_COUNT; ++id) {
    MessageLoop::Start(static_cast<MessageLoop::ID>(id));
  }
  main_message_loop_.reset(new MessageLoop(MessageLoop::UI));
}

void MainRunner::PreMainMessageLoopRun(HINSTANCE instance) {
  MyRegisterClass(instance);
  InitInstance (instance, SW_SHOW);
}

void MainRunner::Run() {
  main_message_loop_->Run();
}

void MainRunner::PostMainMessageLoopRun() {

}

void MainRunner::Shutdown() {
  for (size_t id = MessageLoop::ID_COUNT - 1; id >= MessageLoop::UI + 1; --id) {
    MessageLoop::Stop(static_cast<MessageLoop::ID>(id));
  }
}