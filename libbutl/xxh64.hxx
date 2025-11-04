// file      : libbutl/xxh64.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <iosfwd>      // istream
#include <cstddef>     // size_t
#include <cstdint>
#include <cstring>     // strlen(), memcpy()
#include <type_traits> // enable_if, is_integral

#include <libbutl/export.hxx>

namespace butl
{
  // xxHash variant XXH64 checksum calculator.
  //
  // For a single chunk of data a sum can be obtained in one line, for
  // example:
  //
  // cerr << xxh64 ("123").string () << endl;
  //
  class LIBBUTL_SYMEXPORT xxh64
  {
  public:
    xxh64 () {reset ();}

    // Append binary data.
    //
    void
    append (const void*, std::size_t);

    xxh64 (const void* b, std::size_t n): xxh64 () {append (b, n);}

    // Append string.
    //
    // Note that the hash includes the '\0' terminator. Failed that, a call
    // with an empty string will be indistinguishable from no call at all.
    //
    void
    append (const std::string& s) {append (s.c_str (), s.size () + 1);}

    void
    append (const char* s) {append (s, std::strlen (s) + 1);}

    explicit
    xxh64 (const std::string& s): xxh64 () {append (s);}

    explicit
    xxh64 (const char* s): xxh64 () {append (s);}

    // Append an integral type with a fast path optimization (see
    // XXH64_update_endian() for details). Note that the resulting hash will
    // be endian'ness-dependent.
    //
    void
    append (char c)
    {
      if (state_.memsize + 1 < 32)
      {
        ((std::uint8_t*)state_.mem64)[state_.memsize] =
          static_cast<std::uint8_t> (c);
        state_.memsize++;
        state_.total_len++;
      }
      else
        append (&c, 1);
    }

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value>::type
    append (T x)
    {
      const std::size_t len (sizeof (x));

      if (state_.memsize + len < 32)
      {
        memcpy(((std::uint8_t*)state_.mem64) + state_.memsize, &x, len);
        state_.memsize += (std::uint32_t)len;
        state_.total_len += len;
      }
      else
        append (&x, len);
    }

    // Append stream.
    //
    // Note that currently the stream is expected to be bufstreambuf-based
    // (e.g., ifdstream).
    //
    void
    append (std::istream&);

    explicit
    xxh64 (std::istream& i): xxh64 () {append (i);}

    // Check if any data has been hashed.
    //
    bool
    empty () const {return empty_;}

    // Reset to the default-constructed state.
    //
    void
    reset ();

    // Extract result.
    //
    // It can be obtained as either a uint64_t number, an 8-byte canonical
    // binary representation (the same for LE/BE) or as a 16-character
    // hex-encoded C-string (of the canonical binary representation).
    //
    using digest_type = std::uint8_t[8];

    std::uint64_t
    hash () const;

    const digest_type&
    binary () const;

    const char*
    string () const;

  private:
    struct state // Note: identical to XXH64_state_s.
    {
      std::uint64_t total_len;
      std::uint64_t v1;
      std::uint64_t v2;
      std::uint64_t v3;
      std::uint64_t v4;
      std::uint64_t mem64[4];
      std::uint32_t memsize;
      std::uint32_t reserved[2];
    };

    union
    {
      mutable state state_;
      mutable char buf_[sizeof (state)]; // Also used to store string rep.
    };

    mutable std::uint64_t hash_;
    mutable digest_type bin_;
    mutable bool done_;
    bool empty_;
  };
}
