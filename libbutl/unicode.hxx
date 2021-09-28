// file      : libbutl/unicode.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <ostream>
#include <cstdint> // uint16_t

#include <libbutl/export.hxx>

namespace butl
{
  // Note that the Unicode Standard requires the surrogates ([D800 DFFF]) to
  // only be used in the context of the UTF-16 character encoding form. Thus,
  // we omit the surrogate codepoint type and assume surrogates as invalid
  // codepoints.
  //
  enum class codepoint_types: std::uint16_t
  {
    // Useful to denote invalid codepoints or when building the type set
    // incrementally.
    //
    none          = 0x00,

    graphic       = 0x01, // L(etter), M(ark), N(number), P(uncturation),
                          // S(symbol), Zs(separator, space)
    format        = 0x02,
    control       = 0x04,
    private_use   = 0x08,
    non_character = 0x10,
    reserved      = 0x20,

    any           = 0x3f
  };

  codepoint_types operator&  (codepoint_types,  codepoint_types);
  codepoint_types operator|  (codepoint_types,  codepoint_types);
  codepoint_types operator&= (codepoint_types&, codepoint_types);
  codepoint_types operator|= (codepoint_types&, codepoint_types);

  // Return the codepoint type for a valid codepoint value and none otherwise.
  //
  // Note that the valid codepoint ranges are [0 D800) and (DFFF 10FFFF].
  //
  codepoint_types
  codepoint_type (char32_t);

  // Return the type name for a single codepoint type and empty string for
  // `none` and `any`.
  //
  // Potential future improvements:
  //  - add the none value name parameter ("invalid" by default)
  //  - produce names for type masks ("graphic, format", "any", etc)
  //
  std::string
  to_string (codepoint_types);

  inline std::ostream&
  operator<< (std::ostream& os, codepoint_types ts)
  {
    return os << to_string (ts);
  }
}

#include <libbutl/unicode.ixx>
