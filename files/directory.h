// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_FTL_FILES_DIRECTORY_H_
#define LIB_FTL_FILES_DIRECTORY_H_

#include <string>

#include "lib/ftl/ftl_export.h"

namespace files {

// Returns the current directory. If the current directory cannot be determined,
// this function will terminate the process.
FTL_EXPORT std::string GetCurrentDirectory();

// Returns whether the given path is a directory.
FTL_EXPORT bool IsDirectory(const std::string& path);

// Create a directory at the given path. If necessary, creates any intermediary
// directory.
FTL_EXPORT bool CreateDirectory(const std::string& path);

}  // namespace files

#endif  // LIB_FTL_FILES_DIRECTORY_H_
