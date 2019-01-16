// file      : libbutl/git.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules
#include <libbutl/git.mxx>
#endif

// C includes.

#include <cassert>

#ifndef __cpp_lib_modules
#include <string>

#include <cstddef> // size_t
#endif

// Other includes.

#ifdef __cpp_modules
module butl.git;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules
import std.core;
#endif
import butl.path;
import butl.optional;
import butl.semantic_version
#endif

import butl.utility;    // digit()
import butl.filesystem; // entry_exists()
#else
#include <libbutl/utility.mxx>
#include <libbutl/optional.mxx>
#include <libbutl/filesystem.mxx>
#include <libbutl/semantic-version.mxx>
#endif

using namespace std;

namespace butl
{
  bool
  git_repository (const dir_path& d)
  {
    // .git can be either a directory or a file in case of a submodule or a
    // separate working tree.
    //
    return entry_exists (d / ".git",
                         true /* follow_symlinks */,
                         true /* ignore_errors   */);
  }

  optional<semantic_version>
  git_version (const string& s)
  {
    // There is some variety across platforms in the version
    // representation.
    //
    // Linux:  git version 2.14.3
    // MacOS:  git version 2.10.1 (Apple Git-78)
    // MinGit: git version 2.16.1.windows.1
    //
    if (s.compare (0, 12, "git version ") == 0)
      return parse_semantic_version (s, 12, "" /* build_separators */);

    return nullopt;
  }
}
