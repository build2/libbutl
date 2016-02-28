// file      : butl/sha256.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/sha256>

// C interface for sha256c.
//
#include <stdint.h>
#include <stddef.h> // size_t

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
}
