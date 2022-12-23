// file      : tests/dir-iterator/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <cstddef> // size_t
#include <iostream>

#include <libbutl/path.hxx>
#include <libbutl/path-io.hxx>
#include <libbutl/utility.hxx>    // operator<<(ostream, exception)
#include <libbutl/timestamp.hxx>
#include <libbutl/filesystem.hxx>

#undef NDEBUG
#include <cassert>

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

// Usage: argv[0] [-v] [-i|-d] <dir>
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
// -d
//    Detect dangling symlinks, rather than fail trying to obtain the target
//    type.
//
int
main (int argc, const char* argv[])
{
  assert (argc > 0);

  bool verbose (false);
  bool ignore_dangling (false);
  bool detect_dangling (false);

  int i (1);
  for (; i != argc; ++i)
  {
    string v (argv[i]);

    if (v == "-v")
      verbose = true;
    else if (v == "-i")
      ignore_dangling = true;
    else if (v == "-d")
      detect_dangling = true;
    else
      break;
  }

  if (i != argc - 1)
  {
    cerr << "usage: " << argv[0] << " [-v] [-i] <dir>" << endl;
    return 1;
  }

  assert (!ignore_dangling || !detect_dangling);

  const char* d (argv[i]);

  try
  {
    for (const dir_entry& de:
           dir_iterator (dir_path (d),
                         (ignore_dangling ? dir_iterator::ignore_dangling :
                          detect_dangling ? dir_iterator::detect_dangling :
                                            dir_iterator::no_follow)))
    {
      timestamp mt (de.mtime ());
      timestamp at (de.atime ());

      entry_type lt (de.ltype ());
      entry_type t (lt == entry_type::symlink ? de.type () : lt);

      const path& p (de.path ());
      path fp (de.base () / p);

      entry_time et (t == entry_type::directory
                     ? dir_time (path_cast<dir_path> (fp))
                     : file_time (fp));

      if (mt != timestamp_unknown)
        assert (mt == et.modification);

      if (at != timestamp_unknown)
        assert (mt == et.access);

      if (de.mtime () != timestamp_unknown)
        assert (de.mtime () == et.modification);

      if (de.atime () != timestamp_unknown)
        assert (de.atime () == et.access);

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
