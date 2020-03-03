// file      : tests/path-entry/driver.cxx -*- C++ -*-
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
import butl.utility;    // operator<<(ostream, exception)
import butl.filesystem;
#else
#include <libbutl/utility.mxx>
#include <libbutl/filesystem.mxx>
#endif

using namespace std;
using namespace butl;

// Usage: argv[0] <path>
//
// If path entry exists then print it's type and size (meaningful for the
// regular file only) to STDOUT, and exit with the zero code. Otherwise exit
// with the one code. Don't follow symlink. On failure print the error
// description to STDERR and exit with the two code.
//
int
main (int argc, const char* argv[])
try
{
  assert (argc == 2);

  auto es (path_entry (argv[1]));

  if (!es.first)
    return 1;

  switch (es.second.type)
  {
  case entry_type::unknown: cout << "unknown"; break;
  case entry_type::regular: cout << "regular"; break;
  case entry_type::directory: cout << "directory"; break;
  case entry_type::symlink: cout << "symlink"; break;
  case entry_type::other: cout << "other"; break;
  }

  cout << endl << es.second.size << endl;
  return 0;
}
catch (const system_error& e)
{
  cerr << e << endl;
  return 2;
}
