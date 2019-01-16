// file      : tests/sendmail/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules
#include <iostream>
#include <system_error>
#endif

// Other includes.

#ifdef __cpp_modules
#ifdef __cpp_lib_modules
import std.core;
import std.io;
#endif
import butl.path;
import butl.process;
import butl.utility;  // operator<<(ostream, exception)
import butl.sendmail;
import butl.fdstream;

import butl.optional;     // @@ MOD Clang should not be necessary.
import butl.small_vector; // @@ MOD Clang should not be necessary.
#else
#include <libbutl/path.mxx>
#include <libbutl/process.mxx>
#include <libbutl/utility.mxx>
#include <libbutl/sendmail.mxx>
#endif

using namespace std;
using namespace butl;

// Usage: argv[0] <to>
//
int
main (int argc, const char* argv[])
try
{
  assert (argc == 2);

  sendmail sm ([] (const char* c[], std::size_t n)
               {
                 process::print (cerr, c, n); cerr << endl;
               },
               2,
               "",
               "tests/sendmail/driver",
               {argv[1]});

  sm.out << cin.rdbuf ();
  sm.out.close ();

  if (!sm.wait ())
    return 1; // Assume diagnostics has been issued.
}
catch (const system_error& e)
{
  cerr << argv[0] << ": " << e << endl;
  return 1;
}
