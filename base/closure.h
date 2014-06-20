#ifndef BASE_CLOSURE_H_
#define BASE_CLOSURE_H_

#include "base/closure_internal.h"

namespace base {
class BindStateBase : public RefCountedThreadSafe<BindStateBase> {
protected:
  friend class RefCountedThreadSafe<BindStateBase>;
  virtual ~BindStateBase() {}
};

class BASE_EXPORT ClosureBase {
public:
  bool is_null() const;
  void Reset();
protected:
  typedef void(*InvokeFuncStorage)(void);
  bool Equals(const ClosureBase& other) const;
  explicit ClosureBase(BindStateBase* bind_state);
  ~ClosureBase();
  scoped_refptr<BindStateBase> bind_state_;
  InvokeFuncStorage polymorphic_invoke_;
};

template <typename Runnable, typename BoundArgsType>
struct BindState;

template <typename Runnable>
struct BindState<Runnable, void()> : public BindStateBase {
  typedef IsWeakMethod<false, void> IsWeakCall;
  typedef Invoker<IsWeakCall::value, BindState, typename Runnable::RunType> InvokerType;

  explicit BindState(const Runnable& runnable)
    : runnable_(runnable) {}
  virtual ~BindState() {}
  Runnable runnable_;
};

template <typename Runnable, typename P1>
struct BindState<Runnable, void(P1)> : public BindStateBase {
  typedef IsWeakMethod<Runnable::IsMethod::value, P1> IsWeakCall;
  typedef Invoker<IsWeakCall::value, BindState, typename Runnable::RunType> InvokerType;
  typedef UnwrapTraits<P1> Bound1UnwrapTraits;

  BindState(const Runnable& runnable, const P1& p1)
    : runnable_(runnable)
    , p1_(p1) {
    MaybeRefcount<Runnable::IsMethod::value, P1>::AddRef(p1_);
  }
  virtual ~BindState() {
    MaybeRefcount<Runnable::IsMethod::value, P1>::Release(p1_);
  }
  Runnable runnable_;
  P1 p1_;
};

template <typename Runnable, typename P1, typename P2>
struct BindState<Runnable, void(P1, P2)> : public BindStateBase {
  typedef IsWeakMethod<Runnable::IsMethod::value, P1> IsWeakCall;
  typedef Invoker<IsWeakCall::value, BindState, typename Runnable::RunType> InvokerType;
  typedef UnwrapTraits<P1> Bound1UnwrapTraits;
  typedef UnwrapTraits<P2> Bound2UnwrapTraits;

  BindState(const Runnable& runnable, const P1& p1, const P2& p2)
    : runnable_(runnable)
    , p1_(p1)
    , p2_(p2) {
    MaybeRefcount<Runnable::IsMethod::value, P1>::AddRef(p1_);
  }
  virtual ~BindState() {
    MaybeRefcount<Runnable::IsMethod::value, P1>::Release(p1_);
  }
  Runnable runnable_;
  P1 p1_;
  P2 p2_;
};

template <typename Runnable, typename P1, typename P2, typename P3>
struct BindState<Runnable, void(P1, P2, P3)> : public BindStateBase {
  typedef IsWeakMethod<Runnable::IsMethod::value, P1> IsWeakCall;
  typedef Invoker<IsWeakCall::value, BindState, typename Runnable::RunType> InvokerType;
  typedef UnwrapTraits<P1> Bound1UnwrapTraits;
  typedef UnwrapTraits<P2> Bound2UnwrapTraits;
  typedef UnwrapTraits<P3> Bound3UnwrapTraits;

  BindState(const Runnable& runnable, const P1& p1, const P2& p2, const P3& p3)
    : runnable_(runnable)
    , p1_(p1)
    , p2_(p2)
    , p3_(p3) {
      MaybeRefcount<Runnable::IsMethod::value, P1>::AddRef(p1_);
  }
  virtual ~BindState() {
    MaybeRefcount<Runnable::IsMethod::value, P1>::Release(p1_);
  }
  Runnable runnable_;
  P1 p1_;
  P2 p2_;
  P3 p3_;
};

class Closure : public ClosureBase {
public:
  typedef void(RunType)();

  Closure() : ClosureBase(NULL) {}

  template <typename Runnable, typename BoundArgsType>
  Closure(BindState<Runnable, BoundArgsType>* bind_state)
    : ClosureBase(bind_state) {
    PolymorphicInvoke invoke_func =
      &BindState<Runnable, BoundArgsType>::InvokerType::Run;
    polymorphic_invoke_ = reinterpret_cast<InvokeFuncStorage>(invoke_func);
  }

  bool Equals(const Closure& other) const {
    return ClosureBase::Equals(other);
  }

  void Run() const {
    PolymorphicInvoke f =
      reinterpret_cast<PolymorphicInvoke>(polymorphic_invoke_);
    f(bind_state_.get());
  }
private:
  typedef void(*PolymorphicInvoke)(BindStateBase* bind_state);
};

template <typename Functor>
Closure Bind(Functor functor) {
  typedef BindState<RunnableAdapter<Functor>, void()> BindState;
  return Closure(new BindState(RunnableAdapter<Functor>(functor)));
}

template <typename Functor, typename P1>
Closure Bind(Functor functor, const P1& p1) {
  typedef BindState<RunnableAdapter<Functor>,
    void(typename CallbackParamTraits<P1>::StorageType)> BindState;
  return Closure(new BindState(RunnableAdapter<Functor>(functor), p1));
}

template <typename Functor, typename P1, typename P2>
Closure Bind(Functor functor, const P1& p1, const P2& p2) {
  typedef BindState<RunnableAdapter<Functor>,
    void(typename CallbackParamTraits<P1>::StorageType,
    typename CallbackParamTraits<P2>::StorageType)> BindState;
  return Closure(new BindState(RunnableAdapter<Functor>(functor), p1, p2));
}

template <typename Functor, typename P1, typename P2, typename P3>
Closure Bind(Functor functor, const P1& p1, const P2& p2, const P3& p3) {
  typedef BindState<RunnableAdapter<Functor>,
    void(typename CallbackParamTraits<P1>::StorageType,
    typename CallbackParamTraits<P2>::StorageType,
    typename CallbackParamTraits<P3>::StorageType)> BindState;
  return Closure(new BindState(RunnableAdapter<Functor>(functor), p1, p2, p3));
}
}

#endif