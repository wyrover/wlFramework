#ifndef BASE_CLOSURE_H_
#define BASE_CLOSURE_H_

#include "base/ref_counted.h"

namespace base {
class BindStateBase : public RefCountedThreadSafe<BindStateBase> {
protected:
  friend class RefCountedThreadSafe<BindStateBase>;
  virtual ~BindStateBase() {}
};

class BASE_EXPORT CallbackBase {
public:
  bool is_null() const;
  void Reset();
protected:
  typedef void(*InvokeFuncStorage)(void);
  bool Equals(const CallbackBase& other) const;
  explicit CallbackBase(BindStateBase* bind_state);
  ~CallbackBase();
  scoped_refptr<BindStateBase> bind_state_;
  InvokeFuncStorage polymorphic_invoke_;
};

template <typename Sig>
class Callback;

template <typename Runnable, typename RunType, typename BoundArgsType>
struct BindState;

template<int NumBound, typename Storage, typename RunType>
struct Invoker;

template<bool IsWeakCall, typename ReturnType, typename Runnable,
  typename ArgsType>
struct InvokeHelper;

template <typename Functor>
class RunnableAdapter;

template <typename R>
class RunnableAdapter<R(*)()> {
public:
  typedef R (RunType)();

  explicit RunnableAdapter(R(*function)())
    : function_(function) {}
  R Run() {
    return function_();
  }
private:
  R (*function_)();
};

template <typename ReturnType, typename Runnable>
struct InvokeHelper<false, ReturnType, Runnable, void()> {
  static ReturnType MakeItSo(Runnable runnable) {
    return runnable.Run();
  }
};

template <typename Runnable>
struct InvokeHelper<false, void, Runnable, void()> {
  static void MakeItSo(Runnable runnable) {
    runnable.Run();
  }
};

template <typename StorageType, typename R>
struct Invoker<0, StorageType, R()> {
  typedef R(UnboundRunType)();
  static R Run(BindStateBase* base) {
    StorageType* storage = static_cast<StorageType*>(base);
    return InvokeHelper<StorageType::IsWeakCall, R,
      typename StorageType::RunnableType, void()>::MakeItSo(storage->runnable_);
  }
};

template <typename Runnable, typename RunType>
struct BindState<Runnable, RunType, void()> : public BindStateBase {
  typedef Runnable RunnableType;
  static const bool IsWeakCall = false;
  typedef Invoker<0, BindState, RunType> InvokerType;
  explicit BindState(const Runnable& runnable)
    : runnable_(runnable) {}
  virtual ~BindState() {}
  RunnableType runnable_;
};

template <typename R>
class Callback<R(void)> : public CallbackBase {
public:
  typedef R(RunType)();

  Callback() : CallbackBase(NULL) {}

  template <typename Runnable, typename BindRunType, typename BoundArgsType>
  Callback(BindState<Runnable, BindRunType, BoundArgsType>* bind_state)
    : CallbackBase(bind_state) {
    PolymorphicInvoke invoke_func =
      &BindState<Runnable, BindRunType, BoundArgsType>::InvokerType::Run;
    polymorphic_invoke_ = reinterpret_cast<InvokeFuncStorage>(invoke_func);
  }

  bool Equals(const Callback& other) const {
    return CallbackBase::Equals(other);
  }

  R Run() const {
    PolymorphicInvoke f =
      reinterpret_cast<PolymorphicInvoke>(polymorphic_invoke_);
    return f(bind_state_.get());
  }
private:
  typedef R(*PolymorphicInvoke)(BindStateBase* bind_state);
};

typedef Callback<void(void)> Closure;

template <typename Functor>
Callback<
  typename BindState<
    typename RunnableAdapter<Functor>,
    typename RunnableAdapter<Functor>::RunType,
    void()>::InvokerType::UnboundRunType>
Bind(Functor functor) {
  typedef BindState<RunnableAdapter<Functor>, RunnableAdapter<Functor>::RunType, void()> BindState;
  return Callback<BindState::InvokerType::UnboundRunType>(
    new BindState(RunnableAdapter<Functor>(functor)));
}
}

#endif