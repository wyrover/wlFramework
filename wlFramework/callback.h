#ifndef WL_CALLBACK_H_
#define WL_CALLBACK_H_

namespace wl {

template <typename Functor>
class RunnableAdapter;

template <typename R>
class RunnableAdapter<R(*)()> {
public:
  explicit RunnableAdapter(R(*function)())
    : function_(function) {}

  R Run() {
    return function_();
  }
private:
  R (*function_)();
};

template <typename R, typename T>
class RunnableAdapter<R(T::*)()> {
public:
  explicit RunnableAdapter(R(T::*method))
    : method_(method) {}

  R Run(T* object) {
    return (object->*method_)();
  }
private:
  R (T::*method_)();
};

template <typename R, typename T>
class RunnableAdapter<R(T::*)() const> {
public:
  explicit RunnableAdapter(R(T::*method)() const)
    : method_(method) {}

  R Run(T* object) {
    return (object->*method_)();
  }
private:
  R (T::*method_)() const;
};

template <typename R, typename A1>
class RunnableAdapter<R(*)(A1)> {
public:
  explicit RunnableAdapter(R(*function)(A1))
    : function_(function) {}

  R Run(const A1& a1) {
    return function_(a1);
  }
private:
  R(*function_)(A1);
};

template <typename R, typename T, typename A1>
class RunnableAdapter<R(T::*)(A1)> {
public:
  explicit RunnableAdapter(R(T::*method)(A1))
    : method_(method) {}

  R Run (T* object, const A1& a1) {
    return object->*method_(a1);
  }
private:
  R(T::*method_)(A1);
};

template <typename Sig>
class Callback;

template <typename R>
class Callback<R(void)> {
public:
  Callback() {}

  R Run() const {}
};

typedef Callback<void(void)> Closure;
}
#endif