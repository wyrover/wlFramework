// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used for debugging assertion support.  The Lock class
// is functionally a wrapper around the LockImpl class, so the only
// real intelligence in the class is in the debugging logic.
#include "base/lock.h"

namespace base {

Lock::Lock() {
  ::InitializeCriticalSectionAndSpinCount(&native_handle_, 2000);
}

Lock::~Lock() {
  ::DeleteCriticalSection(&native_handle_);
}

void Lock::Acquire() {
  ::EnterCriticalSection(&native_handle_);
}

void Lock::Release() {
  ::LeaveCriticalSection(&native_handle_);
}

bool Lock::Try() {
  if (::TryEnterCriticalSection(&native_handle_) != FALSE) {
    return true;
  }
  return false;
}

}  // namespace base
