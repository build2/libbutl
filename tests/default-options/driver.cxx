// file      : tests/default-options/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept> // invalid_argument
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
// -a
//    Default arguments are allowed in the options files.
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

    bool
    more ()
    {
      if (peeked_)
        return true;

      if (!eof_)
        eof_ = ifs_.peek () == ifdstream::traits_type::eof ();

      return !eof_;
    }

    string
    peek ()
    {
      assert (more ());

      if (peeked_)
        return *peeked_;

      string s;
      getline (ifs_, s);

      peeked_ = move (s);
      return *peeked_;
    }

    string
    next ()
    {
      assert (more ());

      string s;
      if (peeked_)
      {
        s = move (*peeked_);
        peeked_ = nullopt;
      }
      else
        getline (ifs_, s);

      return s;
    }

  private:
    ifdstream ifs_;
    bool eof_ = false;
    optional<string> peeked_;
  };

  enum class unknow_mode
  {
    stop,
    fail
  };

  class options: public vector<string>
  {
  public:
    bool
    parse (scanner& s, unknow_mode, unknow_mode m)
    {
      bool r (false);
      while (s.more ())
      {
        string a (s.peek ());

        if (a.compare (0, 2, "--") != 0)
        {
          switch (m)
          {
          case unknow_mode::stop: return r;
          case unknow_mode::fail: throw invalid_argument (a);
          }
        }

        s.next ();

        if (a == "--no-default-options")
          no_default_options_ = true;

        push_back (move (a));
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
  bool args (false);
  vector<dir_path> dirs;
  options cmd_ops;
  vector<string> cmd_args;
  bool print_entries (false);
  bool trace (false);

  for (int i (1); i != argc; ++i)
  {
    string a (argv[i]);

    if (a == "-f")
    {
      assert (++i != argc);
      fs.files.push_back (path (argv[i]));
    }
    else if (a == "-d")
    {
      assert (++i != argc);
      dirs.emplace_back (argv[i]);
    }
    else if (a == "-s")
    {
      assert (++i != argc);
      sys_dir = dir_path (argv[i]);
    }
    else if (a == "-h")
    {
      assert (++i != argc);
      home_dir = dir_path (argv[i]);
    }
    else if (a == "-x")
    {
      assert (++i != argc);
      extra_dir = dir_path (argv[i]);
    }
    else if (a == "-a")
    {
      args = true;
    }
    else if (a == "-e")
    {
      print_entries = true;
    }
    else if (a == "-t")
    {
      trace = true;
    }
    else if (a.compare (0, 2, "--") == 0)
      cmd_ops.push_back (move (a));
    else
      cmd_args.push_back (move (a));
  }

  // Deduce a common start directory.
  //
  fs.start = default_options_start (home_dir, dirs);

  // Load and print the default options.
  //
  default_options<options> def_ops;

  try
  {
    def_ops = load_default_options<options, scanner, unknow_mode> (
      sys_dir,
      home_dir,
      extra_dir,
      fs,
      [trace] (const path& f, bool remote, bool overwrite)
      {
        if (trace)
          cerr << (overwrite ? "overwriting " : "loading ")
               << (remote ? "remote " : "local ") << f << endl;
      },
      args);
  }
  catch (const invalid_argument& e)
  {
    cerr << "error: unexpected argument '" << e.what () << "'" << endl;
    return 1;
  }

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

      if (args)
      {
        cout << "|";

        for (const string& a: e.arguments)
        {
          if (&a != &e.arguments[0])
            cout << ' ';

          cout << a;
        }

      }

      cout << (e.remote ? ",true" : ",false") << endl;
    }
  }

  // Merge the options and print the result.
  //
  if (!print_entries)
  {
    options ops (merge_default_options (def_ops, cmd_ops));

    for (const string& o: ops)
      cout << o << endl;

    if (args)
    {
      vector<string> as (merge_default_arguments (def_ops, cmd_args));

      for (const string& a: as)
        cout << a << endl;
    }
  }

  return 0;
}
