// file      : tests/dir-iterator/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <cstddef> // size_t
#include <iostream>
#endif

// Other includes.

#ifdef __cpp_modules_ts
#ifdef __cpp_lib_modules_ts
import std.core;
import std.io;
#endif
import butl.path;
import butl.path_io;
import butl.utility;
import butl.filesystem;
#else
#include <libbutl/path.mxx>
#include <libbutl/path-io.mxx>
#include <libbutl/utility.mxx>    // operator<<(ostream, exception)
#include <libbutl/filesystem.mxx>
#endif

using namespace std;
using namespace butl;

static const char* entry_type_string[] =
{
  "unk", "reg", "dir", "sym", "oth"
};

inline ostream&
operator<< (ostream& os, entry_type e)
{
  return os << entry_type_string[static_cast<size_t> (e)];
}

// Usage: argv[0] [-v] [-i] <dir>
//
// Iterates over a directory filesystem sub-entries, obtains their types and
// target types for symlinks.
//
// -v
//    Print the filesystem entries types and names to STDOUT.
//
// -i
//    Ignore dangling symlinks, rather than fail trying to obtain the target
//    type.
//
int
main (int argc, const char* argv[])
{
  assert (argc > 0);

  bool verbose (false);
  bool ignore_dangling (false);

  int i (1);
  for (; i != argc; ++i)
  {
    string v (argv[i]);

    if (v == "-v")
      verbose = true;
    else if (v == "-i")
      ignore_dangling = true;
    else
      break;
  }

  if (i != argc - 1)
  {
    cerr << "usage: " << argv[0] << " [-v] [-i] <dir>" << endl;
    return 1;
  }

  const char* d (argv[i]);

  try
  {
    for (const dir_entry& de: dir_iterator (dir_path (d), ignore_dangling))
    {
      entry_type lt (de.ltype ());
      entry_type t (lt == entry_type::symlink ? de.type () : lt);
      const path& p (de.path ());

      if (verbose)
      {
        cout << lt << " ";

        if (lt == entry_type::symlink)
          cout << t;
        else
          cout << "   ";

        cout << " " << p << endl;
      }
    }
  }
  catch (const exception& e)
  {
    cerr << e << endl;
    return 1;
  }
}
