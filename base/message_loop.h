#ifndef BASE_MESSAGE_LOOP_H_
#define BASE_MESSAGE_LOOP_H_

#include <queue>
#include <map>
#include "base/closure.h"
#include "base/lock.h"

typedef unsigned long TimeDelta;
typedef unsigned long TimeTicks;

class BASE_EXPORT MessageLoop {
public:
  enum ID {
    UI = 0,
    IO,
    ID_COUNT
  };
  static MessageLoop* current();
  static void Start(ID identifier);
  static void Stop(ID identifier);
  static void PostTask(ID identifier, const base::Closure& task);
  static void PostDelayedTask(ID identifier, const base::Closure& task, TimeDelta delayed_ms);
  static bool CurrentlyOn(ID identifier);

  explicit MessageLoop(ID identifier);
  ~MessageLoop();
  ID id() { return id_; }
  void Run();
  void Quit();
  void PostandSchduleTask(const base::Closure& task, TimeDelta delayed_ms);
  void HandleHaveWorkMessage();
  void HandleTimerMessage(TimeTicks timer_id);
private:
  void InitMessageWnd();
  ATOM atom_;
  HWND message_hwnd_;
  base::Lock tasks_lock_;
  std::queue<base::Closure> tasks_;
  std::queue<base::Closure> work_queue_;
  std::map<TimeTicks, base::Closure> delayed_tasks_;
  ID id_;
};

#endif