// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_FTL_MEMORY_REF_PTR_INTERNAL_H_
#define LIB_FTL_MEMORY_REF_PTR_INTERNAL_H_

#include <utility>

#include "lib/ftl/ftl_export.h"
#include "lib/ftl/macros.h"

namespace ftl {

template <typename T>
class RefPtr;

template <typename T>
FTL_EXPORT RefPtr<T> AdoptRef(T* ptr);

namespace internal {

// This is a wrapper class that can be friended for a particular |T|, if you
// want to make |T|'s constructor private, but still use |MakeRefCounted()|
// (below). (You can't friend partial specializations.) See |MakeRefCounted()|
// and |FRIEND_MAKE_REF_COUNTED()|.
template <typename T>
class MakeRefCountedHelper final {
 public:
  template <typename... Args>
  static RefPtr<T> MakeRefCounted(Args&&... args) {
    return AdoptRef<T>(new T(std::forward<Args>(args)...));
  }
};

}  // namespace internal
}  // namespace ftl

#endif  // LIB_FTL_MEMORY_REF_PTR_INTERNAL_H_
