// file      : tests/default-options/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <string>
#include <vector>
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
import butl.optional;
import butl.fdstream;
import butl.default_options;
#else
#include <libbutl/path.mxx>
#include <libbutl/path-io.mxx>
#include <libbutl/utility.mxx>         // eof()
#include <libbutl/optional.mxx>
#include <libbutl/fdstream.mxx>
#include <libbutl/default-options.mxx>
#endif

using namespace std;
using namespace butl;

// Usage: argv[0] [-f <file>] [-d <start-dir>] [-s <sys-dir>] [-h <home-dir>]
//                [-x <extra-dir>] [-e] [-t] <cmd-options>
//
// Parse default options files, merge them with the command line options, and
// print the resulting options to STDOUT one per line. Note that the options
// instance is a vector of arbitrary strings.
//
// -f
//    Default options file name. Can be specified multiple times.
//
// -d
//    Directory to start the default options files search from. Can be
//    specified multiple times, in which case a common start (parent)
//    directory is deduced.
//
// -s
//    System directory.
//
// -h
//    Home directory.
//
// -x
//    Extra directory.
//
// -e
//    Print the default options entries (rather than the merged options) to
//    STDOUT one per line in the following format:
//
//    <file>,<space-separated-options>,<remote>
//
// -t
//    Trace the default options files search to STDERR.
//
int
main (int argc, const char* argv[])
{
  using butl::optional;

  class scanner
  {
  public:
    scanner (const string& f): ifs_ (f, fdopen_mode::in, ifdstream::badbit) {}

    optional<string>
    next ()
    {
      string s;
      return !eof (getline (ifs_, s)) ? optional<string> (move (s)) : nullopt;
    }

  private:
    ifdstream ifs_;
  };

  enum class unknow_mode
  {
    fail
  };

  class options: public vector<string>
  {
  public:
    bool
    parse (scanner& s, unknow_mode, unknow_mode)
    {
      bool r (false);
      while (optional<string> o = s.next ())
      {
        if (*o == "--no-default-options")
          no_default_options_ = true;

        push_back (move (*o));
        r = true;
      }
      return r;
    }

    void
    merge (const options& o)
    {
      insert (end (), o.begin (), o.end ());
    }

    bool
    no_default_options () const noexcept
    {
      return no_default_options_;
    }

  private:
    bool no_default_options_ = false;
  };

  // Parse and validate the arguments.
  //
  default_options_files fs;
  optional<dir_path> sys_dir;
  optional<dir_path> home_dir;
  optional<dir_path> extra_dir;
  vector<dir_path> dirs;
  options cmd_ops;
  bool print_entries (false);
  bool trace (false);

  for (int i (1); i != argc; ++i)
  {
    string op (argv[i]);

    if (op == "-f")
    {
      assert (++i != argc);
      fs.files.push_back (path (argv[i]));
    }
    else if (op == "-d")
    {
      assert (++i != argc);
      dirs.emplace_back (argv[i]);
    }
    else if (op == "-s")
    {
      assert (++i != argc);
      sys_dir = dir_path (argv[i]);
    }
    else if (op == "-h")
    {
      assert (++i != argc);
      home_dir = dir_path (argv[i]);
    }
    else if (op == "-x")
    {
      assert (++i != argc);
      extra_dir = dir_path (argv[i]);
    }
    else if (op == "-e")
    {
      print_entries = true;
    }
    else if (op == "-t")
    {
      trace = true;
    }
    else
      cmd_ops.push_back (argv[i]);
  }

  // Deduce a common start directory.
  //
  fs.start = default_options_start (home_dir, dirs);

  // Load and print the default options.
  //
  default_options<options> def_ops (
    load_default_options<options, scanner, unknow_mode> (
      sys_dir,
      home_dir,
      extra_dir,
      fs,
      [trace] (const path& f, bool remote, bool overwrite)
      {
        if (trace)
          cerr << (overwrite ? "overwriting " : "loading ")
               << (remote ? "remote " : "local ") << f << endl;
      }));

  if (print_entries)
  {
    for (const default_options_entry<options>& e: def_ops)
    {
      cout << e.file << ',';

      for (const string& o: e.options)
      {
        if (&o != &e.options[0])
          cout << ' ';

        cout << o;
      }

      cout << (e.remote ? ",true" : ",false") << endl;
    }
  }

  // Merge the options and print the result.
  //
  options ops (merge_default_options (def_ops, cmd_ops));

  if (!print_entries)
  {
    for (const string& o: ops)
      cout << o << endl;
  }

  return 0;
}
