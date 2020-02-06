// file      : tests/mventry/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <iostream>
#include <system_error>
#endif

// Other includes.

#ifdef __cpp_modules_ts
#ifdef __cpp_lib_modules_ts
import std.core;
import std.io;
#endif
import butl.path;
import butl.utility;    // operator<<(ostream, exception)
import butl.filesystem;
#else
#include <libbutl/path.mxx>
#include <libbutl/utility.mxx>
#include <libbutl/filesystem.mxx>
#endif

using namespace std;
using namespace butl;

// Usage: argv[0] <old-path> <new-path>
//
// Rename a file, directory or symlink or move it to the specified directory.
// For the later case the destination path must have a trailing directory
// separator. If succeed then exits with the zero code, otherwise prints the
// error descriptions and exits with the one code.
//
int
main (int argc, const char* argv[])
try
{
  assert (argc == 3);

  path from (argv[1]);
  path to   (argv[2]);

  cpflags fl (cpflags::overwrite_permissions | cpflags::overwrite_content);

  if (to.to_directory ())
    mventry_into (from, path_cast<dir_path> (move (to)), fl);
  else
    mventry (from, to, fl);

  return 0;
}
catch (const invalid_path& e)
{
  cerr << e << ": " << e.path << endl;
  return 1;
}
catch (const system_error& e)
{
  cerr << e << endl;
  return 1;
}
