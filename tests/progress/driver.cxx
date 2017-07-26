// file      : tests/progress/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef _WIN32
#  include <thread> // this_thread::sleep_for()
#else
#  include <libbutl/win32-utility.hxx>
#endif

#include <cstddef>  // size_t
#include <iostream>

#include <libbutl/diagnostics.hxx>

using namespace std;
using namespace butl;

int
main ()
{
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
}
