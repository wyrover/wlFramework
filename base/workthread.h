#ifndef BASE_WORKTHREAD_H_
#define BASE_WORKTHREAD_H_

class BASE_EXPORT WorkThread {
public:
  WorkThread();
  ~WorkThread();
  void InitMessageWnd();
  void ScheduleWork();
  void ScheduleDelayedWork();
  void RunLoop();
  bool DoWork();
private:
  static LRESULT CALLBACK WndProcThunk(HWND window_handle,
    UINT message, WPARAM wparam, LPARAM lparam);
  ATOM atom_;
  HWND message_hwnd_;
};

#endif