// file      : tests/dir-iterator/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules
#include <cstddef> // size_t
#include <iostream>
#endif

// Other includes.

#ifdef __cpp_modules
#ifdef __cpp_lib_modules
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

// @@ Should we make the test silent unless -v arg passed. In silen mode could
// compare the output with a set of predefined dir entries.
//
int
main (int argc, const char* argv[])
{
  if (!(argc == 2 || (argc == 3 && argv[1] == string ("-v"))))
  {
    cerr << "usage: " << argv[0] << " [-v] <dir>" << endl;
    return 1;
  }

  bool v (argc == 3);
  const char* d (argv[argc - 1]);

  try
  {
    for (const dir_entry& de: dir_iterator (dir_path (d)))
    {
      entry_type lt (de.ltype ());
      entry_type t (lt == entry_type::symlink ? de.ltype () : lt);
      const path& p (de.path ());

      if (v)
      {
        cerr << lt << " ";

        if (lt == entry_type::symlink)
          cerr << t;
        else
          cerr << "   ";

        cerr << " " << p << endl;
      }
    }
  }
  catch (const exception& e)
  {
    cerr << argv[1] << ": " << e << endl;
    return 1;
  }
}
