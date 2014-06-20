#ifndef BASE_CLOSURE_INTERNAL_H_
#define BASE_CLOSURE_INTERNAL_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/weak_ptr.h"

namespace base {
class BindStateBase;

template <bool v>
struct boolean_type {
  static const bool value = v;
};

typedef boolean_type<true> true_type;
typedef boolean_type<false> false_type;

template <bool IsMethod, typename P1>
struct IsWeakMethod : public false_type {};

template <typename T>
struct IsWeakMethod<true, WeakPtr<T> > : public true_type {};

template <typename T>
struct CallbackParamTraits {
  typedef const T& ForwardType;
  typedef T StorageType;
};

template <typename T, typename D>
struct CallbackParamTraits<scoped_ptr<T, D> > {
  typedef scoped_ptr<T, D> ForwardType;
  typedef scoped_ptr<T, D> StorageType;
};

template <typename T>
T& CallbackForward(T& t) { return t; }

template <typename T, typename D>
scoped_ptr<T, D> CallbackForward(scoped_ptr<T, D>& p) { return p.Pass(); }

template <bool is_method, typename T>
struct MaybeRefcount;

template <typename T>
struct MaybeRefcount<false, T> {
  static void AddRef(const T&) {}
  static void Release(const T&) {}
};

template <typename T>
struct MaybeRefcount<true, T> {
  static void AddRef(const T&) {}
  static void Release(const T&) {}
};

template <typename T>
struct MaybeRefcount<true, T*> {
  static void AddRef(T* o) { o->AddRef(); }
  static void Release(T* o) { o->Release(); }
};

template <typename T>
struct MaybeRefcount<true, const T*> {
  static void AddRef(const T* o) { o->AddRef(); }
  static void Release(const T* o) { o->Release(); }
};

template <typename T>
struct UnwrapTraits {
  typedef const T& ForwardType;
  static ForwardType Unwrap(const T& o) { return o; }
};

template <typename T>
struct UnwrapTraits<scoped_refptr<T> > {
  typedef T* ForwardType;
  static ForwardType Unwrap(const scoped_refptr<T>& o) { return o.get(); }
};

template <typename T>
struct UnwrapTraits<WeakPtr<T> > {
  typedef const WeakPtr<T>& ForwardType;
  static ForwardType Unwrap(const WeakPtr<T>& o) { return o; }
};

template<bool IsWeakCall, typename Storage, typename RunType>
struct Invoker;

template <typename Functor>
class RunnableAdapter;

template<>
class RunnableAdapter<void(*)()> {
public:
  typedef void (RunType)();
  typedef false_type IsMethod;
  explicit RunnableAdapter(void(*function)())
    : function_(function) {}
  void Run() {
    function_();
  }
private:
  void (*function_)();
};

template <typename T>
class RunnableAdapter<void(T::*)()> {
public:
  typedef void (RunType)(T*);
  typedef true_type IsMethod;
  explicit RunnableAdapter(void(T::*method)())
    : method_(method) {}
  void Run(T* object) {
    (object->*method_)();
  }
private:
  void (T::*method_)();
};

template <typename A1>
class RunnableAdapter<void(*)(A1)> {
public:
  typedef void (RunType)(A1);
  typedef false_type IsMethod;
  explicit RunnableAdapter(void(*function)(A1))
    : function_(function) {}
  void Run(typename CallbackParamTraits<A1>::ForwardType a1) {
    function_(CallbackForward(a1));
  }
private:
  void (*function_)(A1);
};

template <typename A1, typename A2>
class RunnableAdapter<void(*)(A1, A2)> {
public:
  typedef void (RunType)(A1, A2);
  typedef false_type IsMethod;
  explicit RunnableAdapter(void(*function)(A1, A2))
    : function_(function) {}
  void Run(typename CallbackParamTraits<A1>::ForwardType a1,
    typename CallbackParamTraits<A2>::ForwardType a2) {
      function_(CallbackForward(a1), CallbackForward(a2));
  }
private:
  void (*function_)(A1, A2);
};

template <typename T, typename A1>
class RunnableAdapter<void(T::*)(A1)> {
public:
  typedef void (RunType)(T*, A1);
  typedef true_type IsMethod;
  explicit RunnableAdapter(void(T::*method)(A1))
    : method_(method) {}
  void Run(T* object, typename CallbackParamTraits<A1>::ForwardType a1) {
    (object->*method_)(CallbackForward(a1));
  }
private:
  void (T::*method_)(A1);
};

template <typename A1, typename A2, typename A3>
class RunnableAdapter<void(*)(A1, A2, A3)> {
public:
  typedef void (RunType)(A1, A2, A3);
  typedef false_type IsMethod;
  explicit RunnableAdapter(void(*function)(A1, A2, A3))
    : function_(function) {}
  void Run(typename CallbackParamTraits<A1>::ForwardType a1,
    typename CallbackParamTraits<A2>::ForwardType a2,
    typename CallbackParamTraits<A3>::ForwardType a3) {
      function_(CallbackForward(a1), CallbackForward(a2), CallbackForward(a3));
  }
private:
  void (*function_)(A1, A2, A3);
};

template <typename T, typename A1, typename A2>
class RunnableAdapter<void(T::*)(A1, A2)> {
public:
  typedef void (RunType)(T*, A1, A2);
  typedef true_type IsMethod;
  explicit RunnableAdapter(void(T::*method)(A1, A2))
    : method_(method) {}
  void Run(T* object, typename CallbackParamTraits<A1>::ForwardType a1,
    typename CallbackParamTraits<A2>::ForwardType a2) {
    (object->*method_)(CallbackForward(a1), CallbackForward(a2));
  }
private:
  void (T::*method_)(A1, A2);
};

template <typename StorageType>
struct Invoker<false, StorageType, void()> {
  static void Run(BindStateBase* base) {
    StorageType* storage = static_cast<StorageType*>(base);
    storage->runnable_.Run();
  }
};

template <typename StorageType, typename X1>
struct Invoker<false, StorageType, void(X1)> {
  static void Run(BindStateBase* base) {
    StorageType* storage = static_cast<StorageType*>(base);
    typedef typename StorageType::Bound1UnwrapTraits Bound1UnwrapTraits;
    typename Bound1UnwrapTraits::ForwardType x1 =
      Bound1UnwrapTraits::Unwrap(storage->p1_);

    storage->runnable_.Run(CallbackForward(x1));
  }
};

template <typename StorageType, typename X1>
struct Invoker<true, StorageType, void(X1)> {
  static void Run(BindStateBase* base) {
    StorageType* storage = static_cast<StorageType*>(base);
    typedef typename StorageType::Bound1UnwrapTraits Bound1UnwrapTraits;
    typename Bound1UnwrapTraits::ForwardType x1 =
      Bound1UnwrapTraits::Unwrap(storage->p1_);

    if (!CallbackForward(x1).get())
      return;
    storage->runnable_.Run(CallbackForward(x1).get());
  }
};

template <typename StorageType, typename X1, typename X2>
struct Invoker<false, StorageType, void(X1, X2)> {
  static void Run(BindStateBase* base) {
    StorageType* storage = static_cast<StorageType*>(base);
    typedef typename StorageType::Bound1UnwrapTraits Bound1UnwrapTraits;
    typedef typename StorageType::Bound2UnwrapTraits Bound2UnwrapTraits;
    typename Bound1UnwrapTraits::ForwardType x1 =
      Bound1UnwrapTraits::Unwrap(storage->p1_);
    typename Bound2UnwrapTraits::ForwardType x2 =
      Bound2UnwrapTraits::Unwrap(storage->p2_);

    storage->runnable_.Run(CallbackForward(x1), CallbackForward(x2));
  }
};

template <typename StorageType, typename X1, typename X2>
struct Invoker<true, StorageType, void(X1, X2)> {
  static void Run(BindStateBase* base) {
    StorageType* storage = static_cast<StorageType*>(base);
    typedef typename StorageType::Bound1UnwrapTraits Bound1UnwrapTraits;
    typedef typename StorageType::Bound2UnwrapTraits Bound2UnwrapTraits;
    typename Bound1UnwrapTraits::ForwardType x1 =
      Bound1UnwrapTraits::Unwrap(storage->p1_);
    typename Bound2UnwrapTraits::ForwardType x2 =
      Bound2UnwrapTraits::Unwrap(storage->p2_);

    if (!CallbackForward(x1).get())
      return;
    storage->runnable_.Run(CallbackForward(x1).get(), CallbackForward(x2));
  }
};

template <typename StorageType, typename X1, typename X2, typename X3>
struct Invoker<false, StorageType, void(X1, X2, X3)> {
  static void Run(BindStateBase* base) {
    StorageType* storage = static_cast<StorageType*>(base);
    typedef typename StorageType::Bound1UnwrapTraits Bound1UnwrapTraits;
    typedef typename StorageType::Bound2UnwrapTraits Bound2UnwrapTraits;
    typedef typename StorageType::Bound3UnwrapTraits Bound3UnwrapTraits;
    typename Bound1UnwrapTraits::ForwardType x1 =
      Bound1UnwrapTraits::Unwrap(storage->p1_);
    typename Bound2UnwrapTraits::ForwardType x2 =
      Bound2UnwrapTraits::Unwrap(storage->p2_);
    typename Bound3UnwrapTraits::ForwardType x3 =
      Bound3UnwrapTraits::Unwrap(storage->p3_);

    storage->runnable_.Run(CallbackForward(x1), CallbackForward(x2), CallbackForward(x3));
  }
};

template <typename StorageType, typename X1, typename X2, typename X3>
struct Invoker<true, StorageType, void(X1, X2, X3)> {
  static void Run(BindStateBase* base) {
    StorageType* storage = static_cast<StorageType*>(base);
    typedef typename StorageType::Bound1UnwrapTraits Bound1UnwrapTraits;
    typedef typename StorageType::Bound2UnwrapTraits Bound2UnwrapTraits;
    typedef typename StorageType::Bound3UnwrapTraits Bound3UnwrapTraits;
    typename Bound1UnwrapTraits::ForwardType x1 =
      Bound1UnwrapTraits::Unwrap(storage->p1_);
    typename Bound2UnwrapTraits::ForwardType x2 =
      Bound2UnwrapTraits::Unwrap(storage->p2_);
    typename Bound3UnwrapTraits::ForwardType x3 =
      Bound3UnwrapTraits::Unwrap(storage->p3_);

    if (!CallbackForward(x1).get())
      return;
    storage->runnable_.Run(CallbackForward(x1).get(), CallbackForward(x2), CallbackForward(x3));
  }
};
}

#endif