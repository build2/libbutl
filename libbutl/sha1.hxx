// file      : libbutl/sha1.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <iosfwd>   // istream
#include <string>
#include <cstddef>  // size_t
#include <cstdint>
#include <cstring>  // strlen()

#include <libbutl/export.hxx>

namespace butl
{
  // SHA1 checksum calculator.
  //
  // For a single chunk of data a sum can be obtained in one line, for
  // example:
  //
  // cerr << sha1 ("123").string () << endl;
  //
  class LIBBUTL_SYMEXPORT sha1
  {
  public:
    sha1 () {reset ();}

    // Append binary data.
    //
    void
    append (const void*, std::size_t);

    sha1 (const void* b, std::size_t n): sha1 () {append (b, n);}

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
    sha1 (const std::string& s): sha1 () {append (s);}

    explicit
    sha1 (const char* s): sha1 () {append (s);}

    // Append stream.
    //
    // Note that currently the stream is expected to be bufstreambuf-based
    // (e.g., ifdstream).
    //
    void
    append (std::istream&);

    explicit
    sha1 (std::istream& i): sha1 () {append (i);}

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
    // It can be obtained as either a 20-byte binary digest or as a 40-
    // character hex-encoded C-string.
    //
    using digest_type = std::uint8_t[20];

    const digest_type&
    binary () const;

    const char*
    string () const;

    std::string
    abbreviated_string (std::size_t n) const
    {
      return std::string (string (), n < 40 ? n : 40);
    }

  private:
    struct context // Note: identical to SHA1_CTX.
    {
      union {
        std::uint8_t b8[20];
        std::uint32_t b32[5];
      } h;
      union {
        std::uint8_t b8[8];
        std::uint64_t b64[1];
      } c;
      union {
        std::uint8_t b8[64];
        std::uint32_t b32[16];
      } m;
      std::uint8_t count;
    };

    union
    {
      mutable context ctx_;
      mutable char buf_[sizeof (context)]; // Also used to store string rep.
    };

    mutable digest_type bin_;
    mutable bool done_;
    bool empty_;
  };
}
