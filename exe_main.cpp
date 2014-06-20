#include "base/closure.h"

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
  int i = 0, j = 0;
  base::Closure closure = base::Bind(&Add1);
  closure.Run();
  closure = base::Bind(&Add2, i);
  closure.Run();
  closure = base::Bind(&Add3, i, j);
  closure.Run();
  scoped_refptr<AddClass1> add_class1(new AddClass1());
  add_class1->TestAdd();
  scoped_ptr<AddClass2> add_class2(new AddClass2());
  add_class2->TestAdd();
  return 0;
}