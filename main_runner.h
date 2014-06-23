#ifndef MAIN_RUNNER_H_
#define MAIN_RUNNER_H_

#include "base/message_loop.h"

class MainRunner {
public:
  static MainRunner* Create();
  MainRunner();
  ~MainRunner();
  void Initilize();
  void PreMainMessageLoopRun(HINSTANCE instance);
  void Run();
  void PostMainMessageLoopRun();
  void Shutdown();
private:
  scoped_ptr<MessageLoop> main_message_loop_;
  DISALLOW_COPY_AND_ASSIGN(MainRunner);
};
#endif