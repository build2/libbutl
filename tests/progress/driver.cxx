// file      : tests/progress/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef _WIN32
#  include <unistd.h> // write()

#  include <thread> // this_thread::sleep_for()
#else
#  include <libbutl/win32-utility.hxx>

#  include <io.h> //_write()
#endif

#include <cstddef>  // size_t
#include <iostream>

#include <libbutl/process.hxx>
#include <libbutl/fdstream.hxx>    // fdnull(), stderr_fd()
#include <libbutl/diagnostics.hxx>

using namespace std;
using namespace butl;

// Usage:
//
// argv[0] [-n] [-c]
//
// -n
//    Do not run child process. By default the program runs itself with -c
//    option (see below).
//
// -c
//    Run as a child process that just prints lines with a small but varying
//    delay.
//
int
main (int argc, const char* argv[])
{
  bool child (false);
  bool no_child (false);

  assert (argc > 0);

  for (int i (1); i != argc; ++i)
  {
    string v (argv[i]);

    if (v == "-c")
      child = true;
    else if (v == "-n")
      no_child = true;
    else
      assert (false);
  }

  auto sleep = [] (size_t ms = 100)
  {
    // MINGW GCC 4.9 doesn't implement this_thread so use Win32 Sleep().
    //
#ifndef _WIN32
    this_thread::sleep_for (chrono::milliseconds (ms));
#else
    Sleep (static_cast<DWORD> (ms));
#endif
  };

  if (child)
  {
    auto print = [] (const string& s)
    {
#ifndef _WIN32
      write (stderr_fd(), s.c_str (), s.size ());
#else
      _write (stderr_fd(), s.c_str (), static_cast<unsigned int> (s.size ()));
#endif
    };

    for (size_t i (50); i != 0; --i)
    {
      print ("Child line " + to_string (i) + '\n');
      sleep (200 - i);
    }

    return 0;
  }

  // @@ Can't compile unless convert to process_env() explicitly (GCC still
  //    warns about calls ambiguity).
  //
  process pr (!no_child
              ? process_start (fdnull (), fdnull (), stderr_fd (),
                               process_env (argv[0]), "-c")
              : process (process_exit (0))); // Exited normally.

  for (size_t i (100); i != 0; --i)
  {
    if (i % 10 == 0)
      diag_stream_lock () << "Line " << i / 10 << endl;

    {
      diag_progress_lock l;
      diag_progress = "  " + to_string (i) + "%";
    }

    sleep ();
  }

  sleep (1000);

  // Test that the progress line is restored by the diag_stream_lock.
  //
  diag_stream_lock () << "Printed to diag_stream" << endl;

  sleep (1000);

  // Test that the progress line is restored after printing to cerr.
  //
  {
    cerr << "Printed to std::cerr" << endl;
    diag_progress_lock ();
  }

  sleep (1000);

  assert (pr.wait ());
}
