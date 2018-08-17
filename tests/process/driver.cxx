// file      : tests/process/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules
#include <ios>
#include <string>
#include <vector>
#include <iterator>  // istreambuf_iterator, ostream_iterator
#include <algorithm> // copy()
#include <iostream>
#endif

// Other includes.

#ifdef __cpp_modules
#ifdef __cpp_lib_modules
import std.core;
import std.io;
#endif
import butl.path;
import butl.utility;  // setenv(), getenv()
import butl.process;
import butl.optional;
import butl.fdstream;
#else
#include <libbutl/path.mxx>
#include <libbutl/utility.mxx>
#include <libbutl/process.mxx>
#include <libbutl/optional.mxx>
#include <libbutl/fdstream.mxx>
#endif

using namespace std;
using namespace butl;

static const char* envvars[] = {"ABC=1", "DEF", nullptr};

static bool
exec (const path& p,
      vector<char> in = vector<char> (),
      bool out = false,
      bool err = false,
      bool pipeline = false,
      bool bin = true,           // Set binary mode for file descriptors.
      dir_path wd = dir_path (), // Set the working dir for the child process.
      bool env = false)          // Set environment variables for the child.
{
  using cstrings = vector<const char*>;
  using butl::optional;

  assert (!in.empty () || (!out && !err)); // Nothing to output if no input.
  assert (!pipeline || out); // To pipeline need to output something.

  const char* cwd (!wd.empty () ? wd.string ().c_str () : nullptr);

  cstrings args {p.string ().c_str (), "-c"};

  if (bin)
    args.push_back ("-b");

  if (env)
  {
    args.push_back ("-e");

    // Here we set the environment variables for the current process to make
    // sure that the child process will not see the variable that is requested
    // to be unset, and will see the other one unaffected.
    //
    setenv ("DEF", "2");
    setenv ("XYZ", "3");
  }

  if (cwd != nullptr)
    args.push_back (cwd);

  args.push_back (nullptr);

  try
  {
    bool r (true);

    // If both o and e are true, then redirect STDERR to STDOUT, so both can be
    // read from the same stream.
    //
    process pr (args.data (),
                !in.empty () ? -1 : -2,
                out ? -1 : -2,
                err ? (out ? 1 : -1) : -2,
                cwd,
                env ? envvars : nullptr);

    try
    {
      if (!in.empty ())
      {
        r = !pr.try_wait (); // Couldn't exit as waiting for the input.

        auto bin_mode = [bin](auto_fd fd) -> auto_fd
        {
          if (bin)
            fdmode (fd.get (), fdstream_mode::binary);

          return fd;
        };

        ofdstream os (bin_mode (move (pr.out_fd)));
        copy (in.begin (), in.end (), ostream_iterator<char> (os));
        os.close ();

        if (out)
        {
          vector<char> o;

          if (pipeline)
          {
            // Here we test both passing process output fd as an input for
            // another process (pr2.in = pr.out), as well as passing process
            // input fd as an output for another one (pr2.out = pr3.in). The
            // overall pipeline looks like 'os -> pr -> pr2 -> pr3 -> is'.
            //
            process pr3 (args.data (),
                         -1, -1, -2,
                         cwd,
                         env ? envvars : nullptr);

            process pr2 (args.data (),
                         pr, bin_mode (move (pr3.out_fd)).get (), -2,
                         cwd,
                         env ? envvars : nullptr);

            ifdstream is (bin_mode (move (pr3.in_ofd)));
            o = is.read_binary ();

            r = pr2.wait () && r;
            r = pr3.wait () && r;
          }
          else
          {
            ifdstream is (bin_mode (move (pr.in_ofd)));
            o = is.read_binary ();
          }

          if (err)
          {
            // If STDERR is redirected to STDOUT then output will be
            // duplicated.
            //
            vector<char> v (in);
            in.reserve (in.size () * 2);
            in.insert (in.end (), v.begin (), v.end ());
          }

          r = in == o && r;
        }

        if (err && !out)
        {
          ifdstream is (bin_mode (move (pr.in_efd)));
          vector<char> e (is.read_binary ());

          r = in == e && r;
        }
      }
    }
    catch (const ios_base::failure&)
    {
      r = false;
    }

    optional<bool> s;
    return pr.wait () && (s = pr.try_wait ()) && *s && r;
  }
  catch (const process_error& e)
  {
    //cerr << args[0] << ": " << e << endl;

    if (e.child)
      exit (1);

    return false;
  }
}

static bool
exec (const path& p,
      const string& i,
      bool o = false,
      bool e = false,
      bool pipeline = false,
      dir_path wd = dir_path (),
      bool env = false)
{
  return exec (
    p, vector<char> (i.begin (), i.end ()), o, e, pipeline, false, wd, env);
}

int
main (int argc, const char* argv[])
{
  using butl::getenv;
  using butl::optional;

  bool child (false);
  bool bin (false);
  dir_path wd;      // Working directory.
  bool env (false); // Check the environment variables.

  assert (argc > 0);

  int i (1);
  for (; i != argc; ++i)
  {
    string v (argv[i]);

    if (v == "-c")
      child = true;
    else if (v == "-b")
      bin = true;
    else if (v == "-e")
      env = true;
    else
    {
      if (!wd.empty ())
        break;

      try
      {
        wd = dir_path (v);
      }
      catch (const invalid_path&)
      {
        break;
      }
    }
  }

  if (i != argc)
  {
    if (!child)
      cerr << "usage: " << argv[0] << " [-c] [-b] [<dir>]" << endl;

    return 1;
  }

  path p;

  try
  {
    p = path (argv[0]);
  }
  catch (const invalid_path&)
  {
    if (child)
      return 1;

    assert (false);
  }

  if (child)
  {
    // Child process. Check if the working directory argument matches the
    // current directory if specified. Read input data if requested, optionally
    // write it to cout and/or cerr.
    //

    if (!wd.empty () && wd.realize () != dir_path::current_directory ())
      return 1;

    if (env)
    {
      // Check that the ABC variable is set, the DEF is unset and the XYZ is
      // left unchanged.
      //
      if (getenv ("ABC") != optional<string> ("1") ||
          getenv ("DEF") ||
          getenv ("XYZ") != optional<string> ("3"))
        return 1;
    }

    try
    {
      if (bin)
      {
        stdin_fdmode  (fdstream_mode::binary);
        stdout_fdmode (fdstream_mode::binary);
        stderr_fdmode (fdstream_mode::binary);
      }

      vector<char> data
        ((istreambuf_iterator<char> (cin)), istreambuf_iterator<char> ());

      cout.exceptions (istream::badbit);
      copy (data.begin (), data.end (), ostream_iterator<char> (cout));

      cerr.exceptions (istream::badbit);
      copy (data.begin (), data.end (), ostream_iterator<char> (cerr));
    }
    catch (const ios_base::failure&)
    {
      return 1;
    }

    return 0;
  }

  dir_path owd (dir_path::current_directory ());

  // Test processes created as "already terminated".
  //
  {
    process p;
    assert (!p.wait ()); // "Terminated" abnormally.
  }

  {
    // Note that if to create as just process(0) then the
    // process(const char* args[], int=0, int=1, int=2) ctor is being called.
    //
    process p (process_exit (0));
    assert (p.wait ()); // "Exited" successfully.
  }

  {
    process p (process_exit (1));
    assert (!p.wait ()); // "Exited" with an error.
  }

  const char* s ("ABC\nXYZ");

  assert (exec (p));
  assert (exec (p, s));
  assert (exec (p, s, true));
  assert (exec (p, s, true, false, true)); // Same but with piping.
  assert (exec (p, s, false, true));
  assert (exec (p, s, true, true));
  assert (exec (p, s, true, true, true)); // Same but with piping.

  // Passing environment variables to the child process.
  //
  assert (exec (p, string (), false, false, false, dir_path (), true));

  // Transmit large binary data through the child.
  //
  vector<char> v;
  v.reserve (5000 * 256);
  for (size_t i (0); i < 5000; ++i)
  {
    char c (numeric_limits<char>::min ());

    do
    {
      v.push_back (c);
    }
    while (c++ != numeric_limits<char>::max ());
  }

  assert (exec (p, v, true, true));
  assert (exec (p, v, true, true, true)); // Same as above but with piping.

  // Execute the child using the full path.
  //
  path fp (p);
  fp.complete ();
  assert (exec (fp));

  // Execute the child using the relative path.
  //
  dir_path::current_directory (fp.directory ());

  assert (exec (dir_path (".") / fp.leaf ()));

  // Fail for non-existent file path.
  //
  assert (!exec (dir_path (".") / path ("dr")));

  // Execute the child using file name having PATH variable being properly set.
  //
  string paths (fp.directory ().string ());

  if (optional<string> p = getenv ("PATH"))
    paths += string (1, path::traits::path_separator) + *p;

  setenv ("PATH", paths);

  dir_path::current_directory (fp.directory () / dir_path (".."));

  assert (exec (fp.leaf ()));

  // Same as above but also with changing the child current directory.
  //
  assert (exec (
    fp.leaf (), vector<char> (), false, false, false, true, fp.directory ()));

#ifndef _WIN32
  // Check that wait() works properly if the underlying low-level wait
  // operation fails.
  //
  process pr;
  pr.handle = reinterpret_cast<process::handle_type> (-1);
  assert (!pr.wait (true) && !pr.wait (false));
#endif

  // Test execution of Windows batch files. The test file is in the original
  // working directory.
  //
#ifdef _WIN32
  {
    assert (exec (owd / "test.bat"));
    assert (exec (owd / "test"));

    paths = owd.string () + path::traits::path_separator + paths;
    setenv ("PATH", paths);

    assert (exec (path ("test.bat")));
    assert (exec (path ("test")));
    assert (!exec (path ("testX.bat")));
  }
#endif
}
