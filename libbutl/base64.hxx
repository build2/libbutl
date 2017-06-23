// file      : libbutl/base64.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_BASE64_HXX
#define LIBBUTL_BASE64_HXX

#include <iosfwd>
#include <string>
#include <vector>

#include <libbutl/export.hxx>

namespace butl
{
  // Base64-encode a stream or a buffer. Split the output into 76 char-long
  // lines (new line is the 77th). If reading from a stream, check if it has
  // badbit, failbit, or eofbit set and throw invalid_argument if that's the
  // case. Otherwise, set eofbit on completion. If writing to a stream, check
  // if it has badbit, failbit, or eofbit set and throw invalid_argument if
  // that's the case. Otherwise set badbit if the write operation fails.
  //
  LIBBUTL_SYMEXPORT void
  base64_encode (std::ostream&, std::istream&);

  LIBBUTL_SYMEXPORT std::string
  base64_encode (std::istream&);

  LIBBUTL_SYMEXPORT std::string
  base64_encode (const std::vector<char>&);

  // Base64-decode a stream or a string. Throw invalid_argument if the input
  // is not a valid base64 representation. If reading from a stream, check if
  // it has badbit, failbit, or eofbit set and throw invalid_argument if
  // that's the case. Otherwise, set eofbit on completion. If writing to a
  // stream, check if it has badbit, failbit, or eofbit set and throw
  // invalid_argument if that's the case. Otherwise set badbit if the write
  // operation fails.
  //
  LIBBUTL_SYMEXPORT void
  base64_decode (std::ostream&, std::istream&);

  LIBBUTL_SYMEXPORT void
  base64_decode (std::ostream&, const std::string&);

  LIBBUTL_SYMEXPORT std::vector<char>
  base64_decode (const std::string&);
}

#endif // LIBBUTL_BASE64_HXX
