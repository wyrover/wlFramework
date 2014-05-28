#include "stdafx.h"

#include "ref_counted.h"

namespace wl {

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
  return AtomicRefCountIsOne(
      &const_cast<RefCountedThreadSafeBase*>(this)->ref_count_);
}

RefCountedThreadSafeBase::RefCountedThreadSafeBase() : ref_count_(0) {
}

RefCountedThreadSafeBase::~RefCountedThreadSafeBase() {
}

void RefCountedThreadSafeBase::AddRef() const {
  InterlockedExchangeAdd(
    reinterpret_cast<volatile long*>(&ref_count_), 1l);
}

bool RefCountedThreadSafeBase::Release() const {
  if (InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&ref_count_), -1l) - 1l == 0) {
    return true;
  }
  return false;
}

}

}
