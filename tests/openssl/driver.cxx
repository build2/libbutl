// file      : tests/openssl/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_lib_modules_ts
#include <vector>
#include <iostream>
#include <iterator>
#include <system_error>
#endif

// Other includes.

#ifdef __cpp_modules_ts
#ifdef __cpp_lib_modules_ts
import std.core;
import std.io;
#endif
import butl.path;
import butl.utility;  // operator<<(ostream, exception)
import butl.openssl;
import butl.process;
import butl.fdstream; // nullfd

import butl.optional;     // @@ MOD Clang should not be necessary.
import butl.small_vector; // @@ MOD Clang should not be necessary.
#else
#include <libbutl/path.mxx>
#include <libbutl/utility.mxx>
#include <libbutl/openssl.mxx>
#include <libbutl/fdstream.mxx>
#endif

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Usage: argv[0]
//
int
main (int, const char* argv[])
try
{
  openssl os (nullfd, path ("-"), 2, path ("openssl"), "rand", 128);

  vector<char> r (os.in.read_binary ());
  os.in.close ();

  return os.wait () && r.size () == 128 ? 0 : 1;
}
catch (const system_error& e)
{
  cerr << argv[0] << ": " << e << endl;
  return 1;
}
