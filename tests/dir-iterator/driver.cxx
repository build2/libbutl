// file      : tests/dir-iterator/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2015 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cstddef> // size_t
#include <cassert>
#include <iostream>

#include <butl/path>
#include <butl/path-io>
#include <butl/filesystem>

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

int
main (int argc, const char* argv[])
{
  if (argc != 2)
  {
    cerr << "usage: " << argv[0] << " <dir>" << endl;
    return 1;
  }

  try
  {
    for (const dir_entry& de: dir_iterator (dir_path (argv[1])))
    {
      cerr << de.ltype () << " ";

      if (de.ltype () == entry_type::symlink)
        cerr << de.type ();
      else
        cerr << "   ";

      cerr << " " << de.path () << endl;
    }
  }
  catch (const exception& e)
  {
    cerr << argv[1] << ": " << e.what () << endl;
    return 1;
  }
}
