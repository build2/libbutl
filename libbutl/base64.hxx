// file      : libbutl/base64.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

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

  // Encode a stream or a buffer using base64url (RFC4648), a base64 variant
  // with different 62nd and 63rd alphabet characters (- and _ instead of ~
  // and .; to make it filesystem safe) and optional padding because the
  // padding character `=` would have to be percent-encoded to be safe in
  // URLs. This implementation does not output any padding, newlines or any
  // other whitespace (which is required, for example, by RFC7519: JSON Web
  // Token (JWT) and RFC7515: JSON Web Signature (JWS)).
  //
  // Note that base64url decoding has not yet been implemented.
  //
  LIBBUTL_SYMEXPORT void
  base64url_encode (std::ostream&, std::istream&);

  LIBBUTL_SYMEXPORT std::string
  base64url_encode (std::istream&);

  LIBBUTL_SYMEXPORT std::string
  base64url_encode (const std::vector<char>&);

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
