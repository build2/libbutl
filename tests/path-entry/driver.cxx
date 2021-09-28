// file      : tests/path-entry/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <iostream>
#include <stdexcept>    // invalid_argument
#include <system_error>

#include <libbutl/path.hxx>
#include <libbutl/path-io.hxx>
#include <libbutl/utility.hxx>    // operator<<(ostream, exception)
#include <libbutl/optional.hxx>
#include <libbutl/timestamp.hxx>
#include <libbutl/filesystem.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Usage: argv[0] [-l] [-t] [-p <permissions>] [-m <time>] [-a <time>] <path>
//
// If path entry exists then optionally modify its meta-information and print
// its type, size (meaningful for the regular file only), target path if the
// specified entry is a symlink and its path otherwise, permissions,
// modification and access times to stdout, one value per line, and exit with
// the zero code. Otherwise exit with the one code. Don't follow symlink by
// default. On failure print the error description to stderr and exit with the
// two code.
//
// -l
//    Follow symlinks.
//
// -t
//    Assume the path is a file and touch it. Implies -l.
//
// -p <permissions>
//    Set path permissions specified in the chmod utility octal form. Implies
//    -l.
//
// -m <time>
//    Set path modification time specified in the "%Y-%m-%d %H:%M:%S%[.N]"
//    format. Implies -l.
//
// -a <time>
//    As -m but set the access time.
//
int
main (int argc, const char* argv[])
{
  string stage;

  try
  {
    using butl::optional;

    bool follow_symlinks (false);
    optional<permissions> perms;
    optional<timestamp> mtime;
    optional<timestamp> atime;
    bool touch (false);

    auto time = [] (const char* v)
    {
      return from_string (v, "%Y-%m-%d %H:%M:%S%[.N]", true /* local */);
    };

    int i (1);
    for (; i != argc; ++i)
    {
      string v (argv[i]);

      if (v == "-l")
        follow_symlinks = true;
      else if (v == "-t")
      {
        touch = true;
        follow_symlinks = true;
      }
      else if (v == "-p")
      {
        assert (++i != argc);
        v = argv[i];

        size_t n;
        perms = static_cast<permissions> (stoull (v, &n, 8));
        assert (n == v.size ());

        follow_symlinks = true;
      }
      else if (v == "-m")
      {
        assert (++i != argc);
        mtime = time (argv[i]);

        follow_symlinks = true;
      }
      else if (v == "-a")
      {
        assert (++i != argc);
        atime = time (argv[i]);

        follow_symlinks = true;
      }
      else
        break;
    }

    assert (i == argc - 1);

    path p (argv[i]);

    if (touch)
    {
      stage = "touch";
      touch_file (p);
    }

    stage = "stat entry";
    pair<bool, entry_stat> es (path_entry (p, follow_symlinks));

    if (!es.first)
      return 1;

    stage = "lstat entry";
    pair<bool, entry_stat> ls (path_entry (p));
    assert (ls.first);

    // The entry is a directory with a symlink followed.
    //
    bool tdir;

    if (follow_symlinks || es.second.type != entry_type::symlink)
      tdir = (es.second.type == entry_type::directory);
    else
    {
      stage = "stat target";
      pair<bool, entry_stat> ts (path_entry (p, true /* follow_symlinks */));

      if (!ts.first)
        return 1;

      tdir = (ts.second.type == entry_type::directory);
    }

    if (perms)
    {
      stage = "set permissions";
      path_permissions (p, *perms);
    }

    if (mtime)
    {
      if (tdir)
      {
        stage = "set directory mtime";
        dir_mtime (path_cast<dir_path> (p), *mtime);
      }
      else
      {
        stage = "set file mtime";
        file_mtime (p, *mtime);
      }
    }

    if (atime)
    {
      if (tdir)
      {
        stage = "set directory atime";
        dir_atime (path_cast<dir_path> (p), *atime);
      }
      else
      {
        stage = "set file atime";
        file_atime (p, *atime);
      }
    }

    cout << "type: ";
    switch (es.second.type)
    {
    case entry_type::unknown:   cout << "unknown"; break;
    case entry_type::regular:   cout << "regular"; break;
    case entry_type::directory: cout << "directory"; break;
    case entry_type::symlink:   cout << "symlink"; break;
    case entry_type::other:     cout << "other"; break;
    }
    cout << endl;

    cout << "size: " << es.second.size << endl
         << "target: "
         << (ls.second.type == entry_type::symlink
             ? readsymlink (p)
             : p) << endl;

    stage = "get permissions";

    cout << "permissions: "
         << oct << static_cast<size_t> (path_permissions (p)) << endl;

    stage = tdir ? "get directory times" : "get file times";

    entry_time et (tdir ? dir_time (path_cast<dir_path> (p)) : file_time (p));
    cout << "mtime: " << et.modification << endl
         << "atime: " << et.access << endl;

    return 0;
  }
  catch (const invalid_argument& e)
  {
    cerr << e << endl;
    return 2;
  }
  catch (const system_error& e)
  {
    cerr << stage << " failed: " << e << endl;
    return 2;
  }
}
