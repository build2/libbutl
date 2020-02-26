// file      : libbutl/unicode.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline codepoint_types
  operator&= (codepoint_types& x, codepoint_types y)
  {
    return x = static_cast<codepoint_types> (
      static_cast<std::uint16_t> (x) &
      static_cast<std::uint16_t> (y));
  }

  inline codepoint_types
  operator|= (codepoint_types& x, codepoint_types y)
  {
    return x = static_cast<codepoint_types> (
      static_cast<std::uint16_t> (x) |
      static_cast<std::uint16_t> (y));
  }

  inline codepoint_types
  operator& (codepoint_types x, codepoint_types y)
  {
    return x &= y;
  }

  inline codepoint_types
  operator| (codepoint_types x, codepoint_types y)
  {
    return x |= y;
  }

  LIBBUTL_SYMEXPORT codepoint_types
  codepoint_type_lookup (char32_t);

  inline codepoint_types
  codepoint_type (char32_t c)
  {
    // Optimize for the common case (printable ASCII characters).
    //
    if (c >= 0x20 && c <= 0x7E)                            // Printable ASCII?
      return codepoint_types::graphic;
    else if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) // Invalid?
      return codepoint_types::none;
    else if ((c & 0xFFFF) >= 0xFFFE)                       // Non-range based?
      return codepoint_types::non_character;
    else
      return codepoint_type_lookup (c);
  }

  inline std::string
  to_string (codepoint_types t)
  {
    // Note that we use the terms from the Unicode standard ("private-use"
    // rather than "private use", "noncharacter" rather than "non-character").
    //
    switch (t)
    {
    case codepoint_types::graphic:       return "graphic";
    case codepoint_types::format:        return "format";
    case codepoint_types::control:       return "control";
    case codepoint_types::private_use:   return "private-use";
    case codepoint_types::non_character: return "noncharacter"; // No dash.
    case codepoint_types::reserved:      return "reserved";
    case codepoint_types::none:
    case codepoint_types::any:           return "";
    }

    return ""; // Types combination.
  }
}
