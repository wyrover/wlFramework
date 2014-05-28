#include "stdafx.h"

#include "lock.h"

namespace wl {
Lock::Lock() {
  ::InitializeCriticalSectionAndSpinCount(&cs_, 2000);
}
Lock::~Lock() {
  ::DeleteCriticalSection(&cs_);
}
}