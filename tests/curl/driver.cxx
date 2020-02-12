// file      : tests/curl/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <iostream>
#include <system_error>
#endif

// Other includes.

#ifdef __cpp_modules_ts
#ifdef __cpp_lib_modules_ts
import std.core;
import std.io;
#endif
import butl.curl;
import butl.path;
import butl.process;
import butl.utility; // operator<<(ostream, exception)
import butl.fdstream;

import butl.optional;     // @@ MOD Clang should not be necessary.
import butl.small_vector; // @@ MOD Clang should not be necessary.
#else
#include <libbutl/curl.mxx>
#include <libbutl/path.mxx>
#include <libbutl/process.mxx>
#include <libbutl/utility.mxx>
#include <libbutl/fdstream.mxx>
#endif

using namespace std;
using namespace butl;

// Usage: argv[0] tftp|http
//

static void
print_cmd (const char* c[], std::size_t n)
{
  cerr << endl;
  process::print (cerr, c, n);
  cerr << endl;
}

static void
tftp ()
{
  string u ("tftp://localhost:55123/test-driver/tftp");

  auto p = print_cmd;

  // GET non-existent.
  //
  {
    curl c (p, nullfd, fdopen_null (), 2, curl::get, u + "/foo");
    assert (!c.wait ());
  }

  // PUT from file.
  //
  {
    curl c (p, path ("foo-src"), nullfd, 2, curl::put, u + "/foo");
    assert (c.wait ());
  }

  // PUT from stream.
  //
  {
    curl c (p, path ("-"), nullfd, 2, curl::put, u + "/bar");
    c.out << "bar" << endl;
    c.out.close ();
    assert (c.wait ());
  }

  // GET to stream.
  //
  {
    curl c (p, nullfd, path ("-"), 2, curl::get, u + "/foo");
    string s;
    getline (c.in, s);
    c.in.close ();
    assert (c.wait () && s == "foo");
  }

  // GET to /dev/null.
  //
  {
    curl c (p, nullfd, fdopen_null (), 2, curl::get, u + "/foo");
    assert (c.wait ());
  }
}

static void
http ()
{
  string u ("https://build2.org");

  auto p = print_cmd;

  // GET non-existent.
  //
  {
    curl c (p, nullfd, fdopen_null (), 2, curl::get, u + "/bogus");
    assert (!c.wait ());
  }

  // GET to /dev/null.
  //
  {
    curl c (p, nullfd, fdopen_null (), 2, curl::get, u);
    assert (c.wait ());
  }

  // POST from stream.
  //
  {
    curl c (p, path ("-"), 1, 2, curl::post, u + "/bogus");
    c.out << "bogus" << endl;
    c.out.close ();
    assert (!c.wait ());
  }
}

int
main (int argc, const char* argv[])
try
{
  assert (argc == 2);

  string a (argv[1]);

  if      (a == "tftp") tftp ();
  else if (a == "http") http ();
  else                  assert (false);
}
catch (const system_error& e)
{
  cerr << argv[0] << ':' << argv[1] << ": " << e << endl;
  return 1;
}
