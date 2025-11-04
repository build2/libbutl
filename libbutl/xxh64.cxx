// file      : libbutl/xxh64.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/xxh64.hxx>

// @@ TODO: define endian (and in lz4.cxx)

#define XXH_PRIVATE_API // Makes API static and includes xxhash.c.
#include "xxhash.h"

#include <cassert>
#include <istream>

#include <libbutl/utility.hxx>      // *case()
#include <libbutl/bufstreambuf.hxx>

using namespace std;

namespace butl
{
  void xxh64::
  reset ()
  {
    // Note: does not fail even though returns XXH_errorcode.
    //
    XXH64_reset (reinterpret_cast<XXH64_state_t*> (buf_), 0);
    done_ = false;
    empty_ = true;
  }

  void xxh64::
  append (const void* b, size_t n)
  {
    if (n != 0)
    {
      assert (b != nullptr);

      // Note: only fails if passed NULL input pointer.
      //
      XXH64_update (reinterpret_cast<XXH64_state_t*> (buf_), b, n);

      if (empty_)
        empty_ = false;
    }
  }

  void xxh64::
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

  uint64_t xxh64::
  hash () const
  {
    if (!done_)
    {
      hash_ = static_cast<uint64_t> (
        XXH64_digest (reinterpret_cast<XXH64_state_t*> (buf_)));

      done_ = true;
      buf_[0] = '\0'; // Indicate we haven't computed the string yet.
    }

    return hash_;
  }

  const xxh64::digest_type& xxh64::
  binary () const
  {
    if (!done_)
      hash ();

    XXH64_canonicalFromHash (reinterpret_cast<XXH64_canonical_t*> (&bin_),
                             static_cast<XXH64_hash_t> (hash_));
    return bin_;
  }

  static const char hex_map[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f'};

  const char* xxh64::
  string () const
  {
    if (!done_ || buf_[0] == '\0')
    {
      binary ();

      for (size_t i (0); i != 8; ++i)
      {
        buf_[i * 2]     = hex_map[bin_[i] >> 4];
        buf_[i * 2 + 1] = hex_map[bin_[i] & 0x0f];
      }

      buf_[16] = '\0';
    }

    return buf_;
  }
}
