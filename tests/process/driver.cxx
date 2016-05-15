// file      : tests/process/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <stdlib.h> // getenv(), setenv(), _putenv_s()

#include <ios>
#include <string>
#include <vector>
#include <cassert>
#include <iostream>
#include <iterator>  // istreambuf_iterator, ostream_iterator
#include <algorithm> // copy()

#include <butl/path>
#include <butl/process>
#include <butl/fdstream>

using namespace std;
using namespace butl;

static bool
exec (const path& p,
      vector<char> in = vector<char> (),
      bool out = false,
      bool err = false,
      bool pipeline = false,
      bool bin = true,           // Set binary mode for file descriptors.
      dir_path wd = dir_path ()) // Set the working dir for the child process.
{
  using cstrings = vector<const char*>;

  assert (!in.empty () || (!out && !err)); // Nothing to output if no input.
  assert (!pipeline || out); // To pipeline need to output something.

  const char* cwd (!wd.empty () ? wd.string ().c_str () : nullptr);

  auto args = [&p, bin, &cwd](bool i, bool o, bool e) -> cstrings
  {
    cstrings a {p.string ().c_str (), "-c"};

    if (i)
      a.push_back ("-i");

    if (o)
      a.push_back ("-o");

    if (e)
      a.push_back ("-e");

    if (bin)
      a.push_back ("-b");

    if (cwd != nullptr)
      a.push_back (cwd);

    a.push_back (nullptr);
    return a;
  };

  try
  {
    bool r (true);
    cstrings a (args (!in.empty (), out, err));

    // If both o and e are true, then redirect STDERR to STDOUT, so both can be
    // read from the same stream.
    //
    process pr (cwd,
                a.data (),
                !in.empty () ? -1 : 0,
                out ? -1 : 1,
                err ? (out ? 1 : -1) : 2);

    try
    {
      if (!in.empty ())
      {
        bool s;
        r = !pr.try_wait (s); // Couldn't exit as waiting for the input.

        auto bin_mode = [bin](int fd) -> int
          {
            if (bin)
              fdmode (fd, fdtranslate::binary);

            return fd;
          };

        ofdstream os (bin_mode (pr.out_fd));
        os.exceptions (ofdstream::badbit);
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
            cstrings a (args (true, true, false));
            process pr3 (cwd, a.data (), -1, -1);
            process pr2 (cwd, a.data (), pr, bin_mode (pr3.out_fd));

            bool cr (fdclose (pr3.out_fd));
            assert (cr);

            ifdstream is (bin_mode (pr3.in_ofd));

            o = vector<char> (
              (istreambuf_iterator<char> (is)), istreambuf_iterator<char> ());

            r = pr2.wait () && r;
            r = pr3.wait () && r;
          }
          else
          {
            ifdstream is (bin_mode (pr.in_ofd));

            o = vector<char> (
              (istreambuf_iterator<char> (is)), istreambuf_iterator<char> ());
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
          ifdstream is (bin_mode (pr.in_efd));

          vector<char> e
            ((istreambuf_iterator<char> (is)), istreambuf_iterator<char> ());

          r = in == e && r;
        }
      }
    }
    catch (const ios_base::failure&)
    {
      r = false;
    }

    bool s;
    return pr.wait () && pr.try_wait (s) && s && r;
  }
  catch (const process_error& e)
  {
    if (e.child ())
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
      dir_path wd = dir_path ())
{
  return exec (
    p, vector<char> (i.begin (), i.end ()), o, e, pipeline, false, wd);
}

int
main (int argc, const char* argv[])
try
{
  bool child (false);
  bool in (false);
  bool out (false);
  bool err (false);
  bool bin (false);
  dir_path wd; // Working directory.

  assert (argc > 0);

  int i (1);
  for (; i != argc; ++i)
  {
    string v (argv[i]);
    if (v == "-c")
      child = true;
    else if (v == "-i")
      in = true;
    else if (v == "-o")
      out = true;
    else if (v == "-e")
      err = true;
    else if (v == "-b")
      bin = true;
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
      cerr << "usage: " << argv[0] << " [-c] [-i] [-o] [-e] [-b] [<dir>]"
           << endl;

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

    if (!wd.empty () && wd.realize () != dir_path::current ())
      return 1;

    if (!in)
      return 0; // Nothing to read, so nothing to write.

    try
    {
      if (bin)
      {
        stdin_fdmode  (fdtranslate::binary);
        stdout_fdmode (fdtranslate::binary);
        stderr_fdmode (fdtranslate::binary);
      }

      vector<char> data
        ((istreambuf_iterator<char> (cin)), istreambuf_iterator<char> ());

      if (out)
      {
        cout.exceptions (istream::badbit);
        copy (data.begin (), data.end (), ostream_iterator<char> (cout));
      }

      if (err)
      {
        cerr.exceptions (istream::badbit);
        copy (data.begin (), data.end (), ostream_iterator<char> (cerr));
      }
    }
    catch (const ios_base::failure&)
    {
      return 1;
    }

    return 0;
  }

  const char* s ("ABC\nXYZ");

  assert (exec (p));
  assert (exec (p, s));
  assert (exec (p, s, true));
  assert (exec (p, s, true, false, true)); // Same but with piping.
  assert (exec (p, s, false, true));
  assert (exec (p, s, true, true));
  assert (exec (p, s, true, true, true)); // Same but with piping.

  // Transmit large binary data through the child.
  //
  vector<char> v;
  v.reserve (5000 * 256);
  for (size_t i (0); i < 5000; ++i)
  {
    for (size_t c (0); c < 256; ++c)
      v.push_back (c);
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
  dir_path::current (fp.directory ());

  assert (exec (path (".") / fp.leaf ()));

  // Fail for unexistent file path.
  //
  assert (!exec (path ("./dr")));

  // Execute the child using file name having PATH variable being properly set.
  //
  string paths (fp.directory ().string ());

  if (char const* s = getenv ("PATH"))
    paths += string (1, path::traits::path_separator) + s;

#ifndef _WIN32
  assert (setenv ("PATH", paths.c_str (), 1) == 0);
#else
  assert (_putenv_s ("PATH", paths.c_str ()) == 0);
#endif

  dir_path::current (fp.directory () / dir_path (".."));

  assert (exec (fp.leaf ()));

  // Same as above but also with changing the child current directory.
  //
  assert (exec (
    fp.leaf (), vector<char> (), false, false, false, true, fp.directory ()));
}
catch (const system_error&)
{
  assert (false);
}
