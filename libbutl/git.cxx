// file      : libbutl/git.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
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
import butl.standard_version
#endif

import butl.utility;    // digit()
import butl.filesystem; // entry_exists()
#else
#include <libbutl/utility.mxx>
#include <libbutl/optional.mxx>
#include <libbutl/filesystem.mxx>
#include <libbutl/standard-version.mxx>
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

  optional<standard_version>
  git_version (const string& s)
  {
    // There is some variety across platforms in the version
    // representation.
    //
    // Linux:  git version 2.14.3
    // MacOS:  git version 2.10.1 (Apple Git-78)
    // MinGit: git version 2.16.1.windows.1
    //
    // We will consider the first 3 version components that follows the
    // common 'git version ' prefix.
    //
    const size_t b (12);
    if (s.compare (0, b, "git version ") == 0)
    {
      size_t i (b);
      size_t n (0);
      for (char c; i != s.size () && (digit (c = s[i]) || c == '.'); ++i)
      {
        if (c == '.' && ++n == 3)
          break;
      }

      // Returns nullopt if fail to parse.
      //
      return parse_standard_version (string (s, b, i - b));
    }

    return nullopt;
  }
}
