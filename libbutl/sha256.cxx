// file      : libbutl/sha256.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/sha256.hxx>

// C interface for sha256c.
//
#include <stdint.h>
#include <stddef.h> // size_t

struct SHA256_CTX
{
  uint32_t state[8];
  uint64_t count;
  uint8_t buf[64];
};

extern "C"
{
  static void SHA256_Init (SHA256_CTX*);
  static void SHA256_Update (SHA256_CTX*, const void*, size_t);
  static void SHA256_Final (uint8_t[32], SHA256_CTX*);

#include "sha256c.c"
}

#include <cctype>    // isxdigit()
#include <cassert>
#include <istream>
#include <stdexcept> // invalid_argument

#include <libbutl/utility.hxx>      // *case()
#include <libbutl/bufstreambuf.hxx>

using namespace std;

namespace butl
{
  void sha256::
  reset ()
  {
    SHA256_Init (reinterpret_cast<SHA256_CTX*> (buf_));
    done_ = false;
    empty_ = true;
  }

  void sha256::
  append (const void* b, size_t n)
  {
    if (n != 0)
    {
      SHA256_Update (reinterpret_cast<SHA256_CTX*> (buf_), b, n);

      if (empty_)
        empty_ = false;
    }
  }

  void sha256::
  append (istream& is)
  {
    bufstreambuf* buf (dynamic_cast<bufstreambuf*> (is.rdbuf ()));
    assert (buf != nullptr);

    while (is.peek () != istream::traits_type::eof () && is.good ())
    {
      size_t n (buf->egptr () - buf->gptr ());
      append (buf->gptr (), n);
      buf->gbump (static_cast<int> (n));
    }
  }

  const sha256::digest_type& sha256::
  binary () const
  {
    if (!done_)
    {
      SHA256_Final (bin_, reinterpret_cast<SHA256_CTX*> (buf_));
      done_ = true;
      buf_[0] = '\0'; // Indicate we haven't computed the string yet.
    }

    return bin_;
  }

  static const char hex_map[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f'};

  const char* sha256::
  string () const
  {
    if (!done_)
      binary ();

    if (buf_[0] == '\0')
    {
      for (size_t i (0); i != 32; ++i)
      {
        buf_[i * 2]     = hex_map[bin_[i] >> 4];
        buf_[i * 2 + 1] = hex_map[bin_[i] & 0x0f];
      }

      buf_[64] = '\0';
    }

    return buf_;
  }

  string
  sha256_to_fingerprint (const string& s)
  {
    auto bad = []() {throw invalid_argument ("invalid SHA256 string");};

    size_t n (s.size ());
    if (n != 64)
      bad ();

    string f;
    f.reserve (n + 31);
    for (size_t i (0); i != n; ++i)
    {
      char c (s[i]);
      if (!isxdigit (c))
        bad ();

      if (i > 0 && i % 2 == 0)
        f += ":";

      f += ucase (c);
    }

    return f;
  }

  string
  fingerprint_to_sha256 (const string& f, size_t rn)
  {
    auto bad = []() {throw invalid_argument ("invalid fingerprint");};

    size_t n (f.size ());
    if (n != 32 * 3 - 1)
      bad ();

    if (rn > 64)
      rn = 64;

    string s;
    s.reserve (rn);

    // Note that we continue to validate the fingerprint after the result is
    // ready.
    //
    for (size_t i (0); i != n; ++i)
    {
      char c (f[i]);
      if ((i + 1) % 3 == 0)
      {
        if (c != ':')
          bad ();
      }
      else
      {
        if (!isxdigit (c))
          bad ();

        if (s.size () != rn)
          s += lcase (c);
      }
    }

    return s;
  }
}
