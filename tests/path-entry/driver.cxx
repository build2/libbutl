// file      : tests/path-entry/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <iostream>
#include <system_error>

#include <libbutl/utility.hxx>    // operator<<(ostream, exception)
#include <libbutl/filesystem.hxx>

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
