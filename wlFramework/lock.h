#ifndef WL_LOCK_H_
#define WL_LOCK_H_

#include "stdafx.h"

namespace wl {
class WL_EXPORT Lock {
public:
  Lock();
  ~Lock();
  void Acquire() {
    ::EnterCriticalSection(&cs_);
  }
  void Release() {
    ::LeaveCriticalSection(&cs_);
  }
  bool Try() {
    if (::TryEnterCriticalSection(*cs_) != FALSE) {
      return true;
    }
    return false;
  }
private:
  CRITICAL_SECTION cs_;
  DISALLOW_COPY_AND_ASSIGN(Lock);
};

class WL_EXPORT AutoLock {
public:
  explicit AutoLock(Lock& lock) : lock_(lock) {
    lock_.Acquire();
  }
  ~AutoLock() {
    lock_.Release();
  }
private:
  Lock& lock_;
  DISALLOW_COPY_AND_ASSIGN(AutoLock);
}
}

#endif