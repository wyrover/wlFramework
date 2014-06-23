#include "base/closure.h"
#include "base/message_loop.h"
#include "main_runner.h"
// 我们支持的bind类型
void Add1() {}
void Add2(int i) {}
void Add3(int i, int j) {}
class AddClass1 : public base::RefCountedThreadSafe<AddClass1> {
public:
  void Add() {}
  void Add1(int i) {}
  void Add2() const {}
  void TestAdd() {
    base::Closure closure = base::Bind(&AddClass1::Add, this);
    closure.Run();
    int i = 0;
    closure = base::Bind(&AddClass1::Add1, this, i);
    closure.Run();
  }
};
class AddClass2 : public base::SupportsWeakPtr<AddClass2> {
public:
  void Add() {}
  void Add1(int i) {}
  void Add2() const {}
  void TestAdd() {
    base::Closure closure = base::Bind(&AddClass2::Add, AsWeakPtr());
    closure.Run();
    int i = 0;
    closure = base::Bind(&AddClass2::Add1, AsWeakPtr(), i);
    closure.Run();
  }
};

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
  scoped_ptr<MainRunner> main_runner(MainRunner::Create());
  main_runner->Initilize();
  main_runner->PreMainMessageLoopRun(instance);
  main_runner->Run();
  main_runner->PostMainMessageLoopRun();
  main_runner->Shutdown();
  return 0;
}