#ifndef BASE_MESSAGE_LOOP_H_
#define BASE_MESSAGE_LOOP_H_

#include <queue>
#include "base/closure.h"
#include "base/lock.h"

typedef unsigned long TimeDelta;
typedef unsigned long TimeTicks;

class BASE_EXPORT MessageLoop {
public:
  enum Type {
    Type_UI = 0,
    Type_IO,
    Type_COUNT
  };
  static MessageLoop* current();
  static void Start(Type type);
  static void PostTask(Type type, const base::Closure& task);
  static void QuitCurrent();
  explicit MessageLoop(Type type);
  ~MessageLoop();
  Type type() { return type_; }
  void Run();
  void Quit();
  void PostTask(const base::Closure& task);
private:
  void DoWork();
  void InitMessageWnd();
  static LRESULT CALLBACK WndProcThunk(HWND window_handle,
    UINT message, WPARAM wparam, LPARAM lparam);
  ATOM atom_;
  HWND message_hwnd_;
  base::Lock tasks_lock_;
  std::queue<base::Closure> tasks_;
  std::queue<base::Closure> work_queue_;
  Type type_;
};

#endif