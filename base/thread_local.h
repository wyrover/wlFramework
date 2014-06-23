// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREAD_LOCAL_H_
#define BASE_THREAD_LOCAL_H_

namespace base {
template <typename Type>
class ThreadLocalPointer {
public:
  ThreadLocalPointer() {
    slot_ = TlsAlloc();
  }
  ~ThreadLocalPointer() {
    TlsFree(slot_);
  }
  Type* Get() {
    return static_cast<Type*>(TlsGetValue(slot_));
  }
  void Set(Type* ptr) {
    TlsSetValue(slot_, ptr);
  }
private:
  unsigned long slot_;
  DISALLOW_COPY_AND_ASSIGN(ThreadLocalPointer<Type>);
};
}

#endif
