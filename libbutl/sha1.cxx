// file      : libbutl/sha1.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules_ts
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

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <string>
#include <cstddef>
#include <cstdint>
#endif

// Other includes.

#ifdef __cpp_modules_ts
module butl.sha1;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules_ts
import std.core;
#endif
#endif

import butl.fdstream;
#else
#include <libbutl/fdstream.mxx>
#endif

using namespace std;

namespace butl
{
  void sha1::
  reset ()
  {
    SHA1_Init (reinterpret_cast<SHA1_CTX*> (buf_));
    done_ = false;
    empty_ = true;
  }

  void sha1::
  append (const void* b, size_t n)
  {
    if (n != 0)
    {
      SHA1_Update (reinterpret_cast<SHA1_CTX*> (buf_), b, n);

      if (empty_)
        empty_ = false;
    }
  }

  void sha1::
  append (ifdstream& is)
  {
    fdbuf* buf (dynamic_cast<fdbuf*> (is.rdbuf ()));
    assert (buf != nullptr);

    while (is.peek () != ifdstream::traits_type::eof () && is.good ())
    {
      size_t n (buf->egptr () - buf->gptr ());
      append (buf->gptr (), n);
      buf->gbump (static_cast<int> (n));
    }
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
