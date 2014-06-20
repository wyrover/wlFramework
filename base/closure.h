#ifndef BASE_CLOSURE_H_
#define BASE_CLOSURE_H_

#include "base/closure_internal.h"

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

template <typename Runnable, typename BoundArgsType>
struct BindState;

template <typename Runnable>
struct BindState<Runnable, void()> : public BindStateBase {
  typedef Runnable RunnableType;
  typedef IsWeakMethod<false, void> IsWeakCall;
  typedef Invoker<0, IsWeakCall::value, BindState, typename Runnable::RunType> InvokerType;
  typedef typename InvokerType::UnboundRunType UnboundRunType;
  explicit BindState(const Runnable& runnable)
    : runnable_(runnable) {}
  virtual ~BindState() {}
  RunnableType runnable_;
};

template <typename Runnable, typename P1>
struct BindState<Runnable, void(P1)> : public BindStateBase {
  typedef Runnable RunnableType;
  typedef IsWeakMethod<Runnable::IsMethod::value, P1> IsWeakCall;
  typedef Invoker<1, IsWeakCall::value, BindState, typename Runnable::RunType> InvokerType;
  typedef typename InvokerType::UnboundRunType UnboundRunType;
  typedef UnwrapTraits<P1> Bound1UnwrapTraits;

  BindState(const Runnable& runnable, const P1& p1)
    : runnable_(runnable)
    , p1_(p1) {
    MaybeRefcount<Runnable::IsMethod::value, P1>::AddRef(p1_);
  }
  virtual ~BindState() {
    MaybeRefcount<Runnable::IsMethod::value, P1>::Release(p1_);
  }
  RunnableType runnable_;
  P1 p1_;
};

template <>
class Callback<void(void)> : public CallbackBase {
public:
  typedef void(RunType)();

  Callback() : CallbackBase(NULL) {}

  template <typename Runnable, typename BoundArgsType>
  Callback(BindState<Runnable, BoundArgsType>* bind_state)
    : CallbackBase(bind_state) {
    PolymorphicInvoke invoke_func =
      &BindState<Runnable, BoundArgsType>::InvokerType::Run;
    polymorphic_invoke_ = reinterpret_cast<InvokeFuncStorage>(invoke_func);
  }

  bool Equals(const Callback& other) const {
    return CallbackBase::Equals(other);
  }

  void Run() const {
    PolymorphicInvoke f =
      reinterpret_cast<PolymorphicInvoke>(polymorphic_invoke_);
    f(bind_state_.get());
  }
private:
  typedef void(*PolymorphicInvoke)(BindStateBase* bind_state);
};

typedef Callback<void(void)> Closure;

template <typename Functor>
Callback<typename BindState<
  typename RunnableAdapter<Functor>,
  void()>::UnboundRunType>
Bind(Functor functor) {
  typedef BindState<RunnableAdapter<Functor>, void()> BindState;
  return Callback<BindState::UnboundRunType>(
    new BindState(RunnableAdapter<Functor>(functor)));
}

template <typename Functor, typename P1>
Callback<typename BindState<
  typename RunnableAdapter<Functor>,
  void(typename CallbackParamTraits<P1>::StorageType)>::UnboundRunType>
Bind(Functor functor, const P1& p1) {
  typedef BindState<RunnableAdapter<Functor>,
    void(typename CallbackParamTraits<P1>::StorageType)> BindState;
  return Callback<typename BindState::UnboundRunType>(
    new BindState(RunnableAdapter<Functor>(functor), p1));
}
}

#endif