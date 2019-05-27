// file      : tests/backtrace/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef _WIN32
#  include <sys/resource.h> // setrlimit()
#endif

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <string>
#include <iostream>
#include <exception>    // set_terminate(), terminate_handler
#include <system_error>
#else
import std.io;
#endif

// Other includes.

#ifdef __cpp_modules_ts
#ifdef __cpp_lib_modules_ts
import std.core;
#endif
import butl.process;
import butl.backtrace;
#else
#include <libbutl/process.mxx>
#include <libbutl/backtrace.mxx>
#endif

using namespace std;
using namespace butl;

namespace test
{
  // Note: is not static to make sure the stack frame is not optimized out.
  //
  int
  func ()
  {
    throw system_error (EINVAL, generic_category ());
  }
}

static terminate_handler def_term_handler;

// Usage: argv[0] [-c]
//
// Print the backtrace for an unhandled exception.
//
// -c
//    Run as a child process that sets up the backtrace-printing terminate
//    handler and throws unhandled exception.
int
main (int argc, const char* argv[])
{
  // Not to cause the testscript to fail due to the abnormal test driver
  // termination, delegate the unhandled exception backtrace printing to the
  // child process.
  //
  bool child (false);
  for (int i (1); i != argc; ++i)
  {
    string o (argv[i]);

    if (o == "-c")
      child = true;
    else
      assert (false);
  }

  if (child)
  {
    // Disable dumping core file on POSIX.
    //
#ifndef _WIN32
    struct rlimit rlim {0, 0};
    assert (setrlimit (RLIMIT_CORE, &rlim) == 0);
#endif

    def_term_handler = set_terminate ([]()
                                      {
                                        cerr << backtrace ();

                                        if (def_term_handler != nullptr)
                                          def_term_handler ();
                                      });

    return test::func ();
  }

  // Report failure with non-zero code if child succeeds.
  //
  return process_run (0, 1, 2, argv[0], "-c") ? 1 : 0;
}
