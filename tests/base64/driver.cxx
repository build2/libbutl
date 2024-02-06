// file      : tests/base64/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <vector>
#include <sstream>

#include <libbutl/base64.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Test base64 encoding and decoding.
//
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

// Test base64url encoding only (decoding not yet implemented).
//
static bool
encode_url (const string& i, const string& o)
{
  istringstream is (i);
  string s (base64url_encode (is));
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
    base64url_encode (os, is);
    r = os.str () == o && is.eof ();
  }

  if (r)
    r = base64url_encode (vector<char> (i.begin (), i.end ())) == o;

  return r;
}


int
main ()
{
  // base64
  //
  assert (encode ("", ""));
  assert (encode ("B", "Qg=="));
  assert (encode ("BX", "Qlg="));
  assert (encode ("BXz", "Qlh6"));
  assert (encode ("BXzS", "Qlh6Uw=="));
  assert (encode ("BXzS@", "Qlh6U0A="));
  assert (encode ("BXzS@#", "Qlh6U0Aj"));
  assert (encode ("BXzS@#/", "Qlh6U0AjLw=="));

  // base64url: no padding in output.
  //
  assert (encode_url ("", ""));
  assert (encode_url ("B", "Qg"));
  assert (encode_url ("BX", "Qlg"));
  assert (encode_url ("BXz", "Qlh6"));
  assert (encode_url ("BXzS", "Qlh6Uw"));
  assert (encode_url ("BXzS@", "Qlh6U0A"));
  assert (encode_url ("BXzS@#", "Qlh6U0Aj"));
  assert (encode_url ("BXzS@#/", "Qlh6U0AjLw"));

  // Multi-line input.
  //
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

  // base64
  //
  const char* r (
"Y2xhc3MgZmRzdHJlYW1fYmFzZQp7CnByb3RlY3RlZDoKICBmZHN0cmVhbV9iYXNlICgpID0gZGVm\n"
"YXVsdDsKICBmZHN0cmVhbV9iYXNlIChpbnQgZmQpOiBidWZfIChmZCkge30KCnByb3RlY3RlZDoK\n"
"ICBmZGJ1ZiBidWZfOwp9Owo=");

  assert (encode (s, r));

  // base64url: no newlines or padding in output.
  //
  r =
"Y2xhc3MgZmRzdHJlYW1fYmFzZQp7CnByb3RlY3RlZDoKICBmZHN0cmVhbV9iYXNlICgpID0gZGVm"
"YXVsdDsKICBmZHN0cmVhbV9iYXNlIChpbnQgZmQpOiBidWZfIChmZCkge30KCnByb3RlY3RlZDoK"
"ICBmZGJ1ZiBidWZfOwp9Owo";

  assert (encode_url (s, r));

  // Test 63rd and 64th characters: `>` maps to `+` or `-`; `?` maps to `/` or
  // `_`.
  //
  assert (encode     (">>>>>>", "Pj4+Pj4+"));
  assert (encode_url (">>>>>>", "Pj4-Pj4-"));
  assert (encode     ("??????", "Pz8/Pz8/"));
  assert (encode_url ("??????", "Pz8_Pz8_"));
}
