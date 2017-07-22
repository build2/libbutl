// file      : libbutl/sha256.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_SHA256_HXX
#define LIBBUTL_SHA256_HXX

#include <string>
#include <cstring>     // strlen(), memcpy()
#include <cstdint>
#include <cstddef>     // size_t
#include <type_traits> // enable_if, is_integral

#include <libbutl/export.hxx>

namespace butl
{
  // SHA256 checksum calculator.
  //
  // For a single chunk of data a sum can be obtained in one line, for
  // example:
  //
  // cerr << sha256 ("123").string () << endl;
  //
  class LIBBUTL_SYMEXPORT sha256
  {
  public:
    sha256 ();

    // Append binary data.
    //
    void
    append (const void*, std::size_t);

    sha256 (const void* b, std::size_t n): sha256 () {append (b, n);}

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
    sha256 (const std::string& s): sha256 () {append (s);}

    explicit
    sha256 (const char* s): sha256 () {append (s);}

    // Append an integral type with a fast path optimization (see
    // SHA256_Update() for details).
    //
    void
    append (char c)
    {
      std::uint32_t r ((ctx_.count >> 3) & 0x3f);

      if (1 < 64 - r)
      {
        ctx_.buf[r] = static_cast<std::uint8_t> (c);
        ctx_.count += 8;
      }
      else
        append (&c, 1);
    }

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value>::type
    append (T x)
    {
      const std::size_t len (sizeof (x));
      std::uint32_t r ((ctx_.count >> 3) & 0x3f);

      if (len < 64 - r)
      {
        std::memcpy (&ctx_.buf[r], &x, sizeof (x));
        ctx_.count += len << 3;
      }
      else
        append (&x, len);
    }

    // Extract result.
    //
    // It can be obtained as either a 32-byte binary digest or as a 64-
    // character hex-encoded C-string.
    //
    using digest_type = std::uint8_t[32];

    const digest_type&
    binary () const;

    const char*
    string () const;

  public:
    struct context
    {
      std::uint32_t state[8];
      std::uint64_t count;
      std::uint8_t buf[64];
    };

  private:
    union
    {
      mutable context ctx_;
      mutable char str_[65];
    };

    mutable digest_type bin_;
    mutable bool done_;
  };

  // Convert a SHA256 string representation (64 hex digits) to the fingerprint
  // canonical representation (32 colon-separated upper case hex digit pairs,
  // like 01:AB:CD:...). Throw invalid_argument if the argument is not a valid
  // SHA256 string.
  //
  LIBBUTL_SYMEXPORT std::string
  sha256_to_fingerprint (const std::string&);

  // Convert a fingerprint (32 colon-separated hex digit pairs) to the SHA256
  // string representation (64 lower case hex digits). Throw invalid_argument
  // if the argument is not a valid fingerprint.
  //
  LIBBUTL_SYMEXPORT std::string
  fingerprint_to_sha256 (const std::string&);
}

#endif // LIBBUTL_SHA256_HXX
