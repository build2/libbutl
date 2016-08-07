// file      : butl/sha256.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/sha256>

// C interface for sha256c.
//
#include <stdint.h>
#include <stddef.h> // size_t

#include <cctype>    // isxdigit()
#include <stdexcept> // invalid_argument

#include <butl/utility> // ucase(), lcase()

using SHA256_CTX = butl::sha256::context;

extern "C"
{
  static void SHA256_Init (SHA256_CTX*);
  static void SHA256_Update (SHA256_CTX*, const void*, size_t);
  static void SHA256_Final (uint8_t[32], SHA256_CTX*);

#include "sha256c.c"
}

using namespace std;

namespace butl
{
  // sha256
  //
  sha256::
  sha256 ()
      : done_ (false)
  {
    SHA256_Init (&ctx_);
  }

  void sha256::
  append (const void* b, size_t n)
  {
    SHA256_Update (&ctx_, b, n);
  }

  const sha256::digest_type& sha256::
  binary () const
  {
    if (!done_)
    {
      SHA256_Final (bin_, &ctx_);
      done_ = true;
      str_[0] = '\0'; // Indicate we haven't computed the string yet.
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

    if (str_[0] == '\0')
    {
      for (size_t i (0); i != 32; ++i)
      {
        str_[i * 2]     = hex_map[bin_[i] >> 4];
        str_[i * 2 + 1] = hex_map[bin_[i] & 0x0f];
      }

      str_[64] = '\0';
    }

    return str_;
  }

  // Conversion functions
  //
  string
  sha256_to_fingerprint (const string& s)
  {
    auto bad = []() {throw invalid_argument ("invalid SHA256 string");};

    size_t n (s.size ());
    if (n != 64)
      bad ();

    string f;
    f.reserve (n + 31);
    for (size_t i (0); i < n; ++i)
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
  fingerprint_to_sha256 (const string& f)
  {
    auto bad = []() {throw invalid_argument ("invalid fingerprint");};

    size_t n (f.size ());
    if (n != 32 * 3 - 1)
      bad ();

    string s;
    s.reserve (64);
    for (size_t i (0); i < n; ++i)
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

        s += lcase (c);
      }
    }

    return s;
  }
}
