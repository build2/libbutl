// file      : tests/default-options/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <limits>
#include <string>
#include <vector>
#include <iostream>
#include <exception>
#include <stdexcept> // invalid_argument

#include <libbutl/path.hxx>
#include <libbutl/path-io.hxx>
#include <libbutl/utility.hxx>         // eof()
#include <libbutl/optional.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/default-options.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Usage: argv[0] [-f <file>] [-d <start-dir>] [-s <sys-dir>] [-h <home-dir>]
//                [-x <extra-dir>] [-a] [-e] [-t] <cmd-options>
//
// Parse default options files, merge them with the command line options, and
// print the resulting options to STDOUT one per line. Note that the options
// instance is a vector of arbitrary strings.
//
// -f <file>
//    Default options file name. Can be specified multiple times.
//
// -d <start-dir>
//    Directory to start the default options files search from. Can be
//    specified multiple times, in which case a common start (parent)
//    directory is deduced.
//
// -s <sys-dir>
//    System directory.
//
// -h <home-dir>
//    Home directory.
//
// -x <extra-dir>
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
// -m <num>
//    Maximum number of arguments globally (SIZE_MAX/2 by default).
//
// -l <num>
//    Maximum number of arguments in the options file (1024 by default).
//
int
main (int argc, const char* argv[])
{
  using butl::optional;

  class scanner
  {
  public:
    scanner (const string& f, const string& option, size_t pos)
        : option_ (option), start_pos_ (pos) {load (path (f));}

    bool
    more ()
    {
      return i_ < args_.size ();
    }

    string
    peek ()
    {
      assert (more ());
      return args_[i_];
    }

    string
    next ()
    {
      assert (more ());
      return args_[i_++];
    }

    size_t
    position ()
    {
      return start_pos_ + i_;
    }

  private:
    void
    load (const path& f)
    {
      ifdstream is (f, fdopen_mode::in, ifdstream::badbit);

      for (string l; !eof (getline (is, l)); )
      {
        if (option_ && *option_ == l)
        {
          assert (!eof (getline (is, l)));

          // If the path of the file being parsed is not simple and the path
          // of the file that needs to be loaded is relative, then complete
          // the latter using the former as a base.
          //
          path p (l);

          if (!f.simple () && p.relative ())
            p = f.directory () / p;

          load (p);
        }
        else
          args_.push_back (move (l));
      }
    }

  private:
    optional<string> option_;
    vector<string> args_;
    size_t i_ = 0;
    size_t start_pos_;
  };

  enum class unknow_mode
  {
    stop,
    fail
  };

  class unknown_argument: public std::exception
  {
  public:
    string argument;

    explicit
    unknown_argument (string a): argument (move (a)) {}
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
          case unknow_mode::fail: throw unknown_argument (move (a));
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
  size_t arg_max (numeric_limits<size_t>::max () / 2);
  size_t arg_max_file (1024);

  auto num = [] (const string& s) -> size_t
  {
    assert (!s.empty ());

    char* e (nullptr);
    errno = 0; // We must clear it according to POSIX.
    uint64_t r (strtoull (s.c_str (), &e, 10)); // Can't throw.

    assert (errno != ERANGE             &&
            e == s.c_str () + s.size () &&
            r <= numeric_limits<size_t>::max ());

    return static_cast<size_t> (r);
  };

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
    else if (a == "-m")
    {
      assert (++i != argc);
      arg_max = num (argv[i]);
    }
    else if (a == "-l")
    {
      assert (++i != argc);
      arg_max_file = num (argv[i]);
    }
    else if (a.compare (0, 2, "--") == 0)
      cmd_ops.push_back (move (a));
    else
      cmd_args.push_back (move (a));
  }

  // Deduce a common start directory.
  //
  fs.start = default_options_start (home_dir, dirs.begin (), dirs.end ());

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
      "--options-file",
      arg_max,
      arg_max_file,
      args);
  }
  catch (const unknown_argument& e)
  {
    cerr << "error: unexpected argument '" << e.argument << "'" << endl;
    return 1;
  }
  catch (const invalid_argument& e)
  {
    cerr << "error: unable to load default options files: " << e.what ()
         << endl;
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
