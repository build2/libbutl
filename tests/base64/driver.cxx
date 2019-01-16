// file      : tests/base64/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules
#include <string>
#include <vector>
#include <sstream>
#endif

// Other includes.

#ifdef __cpp_modules
#ifdef __cpp_lib_modules
import std.core;
import std.io;
#endif
import butl.base64;
#else
#include <libbutl/base64.mxx>
#endif

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

  const char* s (R"delim(
class fdstream_base
{
protected:
  fdstream_base () = default;
  fdstream_base (int fd): buf_ (fd) {}

protected:
  fdbuf buf_;
};

class ifdstream: fdstream_base, public std::istream
{
public:
  ifdstream (): std::istream (&buf_) {}
  ifdstream (int fd): fdstream_base (fd), std::istream (&buf_) {}

  void close () {buf_.close ();}
  void open (int fd) {buf_.open (fd);}
  bool is_open () const {return buf_.is_open ();}
};
)delim");

  const char* r (R"delim(
CmNsYXNzIGZkc3RyZWFtX2Jhc2UKewpwcm90ZWN0ZWQ6CiAgZmRzdHJlYW1fYmFzZSAoKSA9IGRl
ZmF1bHQ7CiAgZmRzdHJlYW1fYmFzZSAoaW50IGZkKTogYnVmXyAoZmQpIHt9Cgpwcm90ZWN0ZWQ6
CiAgZmRidWYgYnVmXzsKfTsKCmNsYXNzIGlmZHN0cmVhbTogZmRzdHJlYW1fYmFzZSwgcHVibGlj
IHN0ZDo6aXN0cmVhbQp7CnB1YmxpYzoKICBpZmRzdHJlYW0gKCk6IHN0ZDo6aXN0cmVhbSAoJmJ1
Zl8pIHt9CiAgaWZkc3RyZWFtIChpbnQgZmQpOiBmZHN0cmVhbV9iYXNlIChmZCksIHN0ZDo6aXN0
cmVhbSAoJmJ1Zl8pIHt9CgogIHZvaWQgY2xvc2UgKCkge2J1Zl8uY2xvc2UgKCk7fQogIHZvaWQg
b3BlbiAoaW50IGZkKSB7YnVmXy5vcGVuIChmZCk7fQogIGJvb2wgaXNfb3BlbiAoKSBjb25zdCB7
cmV0dXJuIGJ1Zl8uaXNfb3BlbiAoKTt9Cn07Cg==)delim");

  assert (encode (s, r + 1));
}
