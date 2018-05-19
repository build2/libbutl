// file      : libbutl/sha1.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules
#include <libbutl/sha1.mxx>
#endif

// C interface for sha1c.
//
#include <stdint.h>
#include <stddef.h> // size_t

struct sha1_ctxt {
  union {
    uint8_t b8[20];
    uint32_t b32[5];
  } h;
  union {
    uint8_t b8[8];
    uint64_t b64[1];
  } c;
  union {
    uint8_t b8[64];
    uint32_t b32[16];
  } m;
  uint8_t count;
};
typedef struct sha1_ctxt SHA1_CTX;

extern "C"
{
  static void sha1_init(struct sha1_ctxt *);
  static void sha1_pad(struct sha1_ctxt *);
  static void sha1_loop(struct sha1_ctxt *, const uint8_t *, size_t);
  static void sha1_result(struct sha1_ctxt *, char[20]);

#include "sha1.c"
}

#define SHA1_Init(x)         sha1_init((x))
#define SHA1_Update(x, y, z) sha1_loop((x), (const uint8_t *)(y), (z))
#define SHA1_Final(x, y)     sha1_result((y), (char(&)[20])(x))

#ifndef __cpp_lib_modules
#include <string>
#include <cstddef>
#include <cstdint>
#endif

// Other includes.

#ifdef __cpp_modules
module butl.sha1;

#ifdef __clang__
#ifdef __cpp_lib_modules
import std.core;
#endif
#endif

#endif

using namespace std;

namespace butl
{
  sha1::
  sha1 ()
      : done_ (false)
  {
    SHA1_Init (reinterpret_cast<SHA1_CTX*> (buf_));
  }

  void sha1::
  append (const void* b, size_t n)
  {
    SHA1_Update (reinterpret_cast<SHA1_CTX*> (buf_), b, n);
  }

  const sha1::digest_type& sha1::
  binary () const
  {
    if (!done_)
    {
      SHA1_Final (bin_, reinterpret_cast<SHA1_CTX*> (buf_));
      done_ = true;
      buf_[0] = '\0'; // Indicate we haven't computed the string yet.
    }

    return bin_;
  }

  static const char hex_map[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f'};

  const char* sha1::
  string () const
  {
    if (!done_)
      binary ();

    if (buf_[0] == '\0')
    {
      for (size_t i (0); i != 20; ++i)
      {
        buf_[i * 2]     = hex_map[bin_[i] >> 4];
        buf_[i * 2 + 1] = hex_map[bin_[i] & 0x0f];
      }

      buf_[40] = '\0';
    }

    return buf_;
  }
}
