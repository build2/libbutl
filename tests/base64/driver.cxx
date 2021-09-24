// file      : tests/base64/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_lib_modules_ts
#include <string>
#include <vector>
#include <sstream>
#endif

// Other includes.

#ifdef __cpp_modules_ts
#ifdef __cpp_lib_modules_ts
import std.core;
import std.io;
#endif
import butl.base64;
#else
#include <libbutl/base64.mxx>
#endif

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

static bool
encode (const string& i, const string& o)
{
  // Test encoding.
  //
  istringstream is (i);
  string s (base64_encode (is));
  bool r (s == o && is.eof ());

  if (r)
  {
    is.seekg (0);

    // VC15 seekg() doesn't clear eofbit.
    //
#if defined(_MSC_VER) && _MSC_VER < 1920
    is.clear ();
#endif

    assert (!is.eof ());

    ostringstream os;
    base64_encode (os, is);
    r = os.str () == o && is.eof ();
  }

  if (r)
    r = base64_encode (vector<char> (i.begin (), i.end ())) == o;

  // Test decoding.
  //
  if (r)
  {
    istringstream is (o);
    ostringstream os;
    base64_decode (os, is);
    r = os.str () == i;
  }

  if (r)
  {
    ostringstream os;
    base64_decode (os, o);
    r = os.str () == i;
  }

  if (r)
  {
    vector<char> v (base64_decode (o));
    r = string (v.begin (), v.end ()) == i;
  }

  return r;
}

int
main ()
{
  assert (encode ("", ""));
  assert (encode ("B", "Qg=="));
  assert (encode ("BX", "Qlg="));
  assert (encode ("BXz", "Qlh6"));
  assert (encode ("BXzS", "Qlh6Uw=="));
  assert (encode ("BXzS@", "Qlh6U0A="));
  assert (encode ("BXzS@#", "Qlh6U0Aj"));
  assert (encode ("BXzS@#/", "Qlh6U0AjLw=="));

  const char* s (
"class fdstream_base\n"
"{\n"
"protected:\n"
"  fdstream_base () = default;\n"
"  fdstream_base (int fd): buf_ (fd) {}\n"
"\n"
"protected:\n"
"  fdbuf buf_;\n"
"};\n");

  const char* r (
"Y2xhc3MgZmRzdHJlYW1fYmFzZQp7CnByb3RlY3RlZDoKICBmZHN0cmVhbV9iYXNlICgpID0gZGVm\n"
"YXVsdDsKICBmZHN0cmVhbV9iYXNlIChpbnQgZmQpOiBidWZfIChmZCkge30KCnByb3RlY3RlZDoK\n"
"ICBmZGJ1ZiBidWZfOwp9Owo=");

  assert (encode (s, r));
}
