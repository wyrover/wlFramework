// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"

namespace base {

namespace subtle {

RefCountedBase::RefCountedBase()
    : ref_count_(0)
    {
}

RefCountedBase::~RefCountedBase() {
}

void RefCountedBase::AddRef() const {
  ++ref_count_;
}

bool RefCountedBase::Release() const {
  if (--ref_count_ == 0) {
    return true;
  }
  return false;
}

bool RefCountedThreadSafeBase::HasOneRef() const {
  return ref_count_ == 1;
}

RefCountedThreadSafeBase::RefCountedThreadSafeBase() : ref_count_(0) {
}

RefCountedThreadSafeBase::~RefCountedThreadSafeBase() {
}

void RefCountedThreadSafeBase::AddRef() const {
  InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(&ref_count_),
    static_cast<LONG>(1));
}

bool RefCountedThreadSafeBase::Release() const {
  if (InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(&ref_count_),
    static_cast<LONG>(-1)) == 1) {
    return true;
  }
  return false;
}

}  // namespace subtle

}  // namespace base
