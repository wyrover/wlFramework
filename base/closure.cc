#include "base/closure.h"

namespace base {

bool ClosureBase::is_null() const {
  return bind_state_.get() == NULL;
}

void ClosureBase::Reset() {
  polymorphic_invoke_ = NULL;
  bind_state_ = NULL;
}

bool ClosureBase::Equals(const ClosureBase& other) const {
  return bind_state_.get() == other.bind_state_.get()
    && polymorphic_invoke_ == other.polymorphic_invoke_;
}

ClosureBase::ClosureBase(BindStateBase* bind_state)
  : bind_state_(bind_state)
  , polymorphic_invoke_(NULL) {

}

ClosureBase::~ClosureBase() {}
}