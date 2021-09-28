// file      : tests/entry-time/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <chrono>
#include <iostream>

#include <libbutl/path.hxx>
#include <libbutl/optional.hxx>
#include <libbutl/timestamp.hxx>
#include <libbutl/filesystem.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Usage: argv[0] (-p|-s <time>) (-f|-d) (-m|-a) <path>
//
// Prints or sets the modification or access time for the specified filesystem
// entry.
//
// -p
//    Print the filesystem entry time as a number of milliseconds passed
//    since UNIX epoch.
//
// -s
//    Set the filesystem entry time that is represented as a number of
//    milliseconds passed since UNIX epoch.
//
// -f
//    The specified path is a file.
//
// -d
//    The specified path is a directory.
//
// -m
//    Print or set the filesystem entry modification time.
//
// -a
//    Print or set the filesystem entry access time.
//
int
main (int argc, const char* argv[])
{
  using butl::optional;

  optional<bool> set;
  optional<bool> dir;
  optional<bool> mod;
  optional<path> p;
  timestamp tm;

  assert (argc > 0);

  for (int i (1); i != argc; ++i)
  {
    string v (argv[i]);

    if (v == "-p")
    {
      assert (!set);
      set = false;
    }
    else if (v == "-s")
    {
      assert (!set);
      set = true;

      assert (i + 1 != argc);
      tm = timestamp (chrono::duration_cast<duration> (
                        chrono::milliseconds (stoull (argv[++i]))));
    }
    else if (v == "-f")
    {
      assert (!dir);
      dir = false;
    }
    else if (v == "-d")
    {
      assert (!dir);
      dir = true;
    }
    else if (v == "-m")
    {
      assert (!mod);
      mod = true;
    }
    else if (v == "-a")
    {
      assert (!mod);
      mod = false;
    }
    else
    {
      assert (set && dir && mod && !p);
      p = path (move (v));
    }
  }

  assert (set);
  assert (dir);
  assert (mod);
  assert (p);

  if (*set)
  {
    if (*dir)
    {
      if (*mod)
        dir_mtime (path_cast<dir_path> (*p), tm);
      else
        dir_atime (path_cast<dir_path> (*p), tm);
    }
    else
    {
      if (*mod)
        file_mtime (*p, tm);
      else
        file_atime (*p, tm);
    }
  }
  else
  {
    if (*dir)
      tm = *mod
        ? dir_mtime (path_cast<dir_path> (*p))
        : dir_atime (path_cast<dir_path> (*p));
    else
      tm = *mod ? file_mtime (*p) : file_atime (*p);

    cout << chrono::duration_cast<chrono::milliseconds> (
      tm.time_since_epoch ()).count () << endl;
  }
}
