// file      : tests/utf8/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <string>
#endif

// Other includes.

#ifdef __cpp_modules_ts
#ifdef __cpp_lib_modules_ts
import std.core;
#endif
import butl.utility;
#else
#include <libbutl/utility.mxx>
#endif

using namespace std;
using namespace butl;

int
main ()
{
  // Valid sequences.
  //
  // Empty.
  //
  assert (utf8 (""));

  // 1 code point.
  //
  assert (utf8 ("a"));                // 1 byte.
  assert (utf8 ("\xD0\xB0"));         // 2 bytes.
  assert (utf8 ("\xE4\xBA\x8C"));     // 3 bytes.
  assert (utf8 ("\xF0\x90\x8C\x82")); // 4 bytes.

  // Multiple code points.
  //
  assert (utf8 ("a\xD0\xB0\xE4\xBA\x8C\xF0\x90\x8C\x82"));

  // Ill-formed sequences.
  //
  // 2-byte sequences.
  //
  assert (!utf8 ("\xC1\x80")); // Invalid first byte.
  assert (!utf8 ("\xD0y"));    // Invalid second byte.

  // 3-byte sequences.
  //
  assert (!utf8 ("\xE2\x70\x80")); // Invalid second byte.
  assert (!utf8 ("\xE2\x80\x70")); // Invalid third byte.

  assert (!utf8 ("\xED\xA0\x80")); // Min UTF-16 surrogate value.
  assert (!utf8 ("\xED\xBF\xBF")); // Max UTF-16 surrogate value.

  // 4-byte sequences.
  //
  assert (!utf8 ("\xF5\x80\x80\x80")); // Invalid first byte.
  assert (!utf8 ("\xF0\x80\x80\x80")); // Invalid second byte.
  assert (!utf8 ("\xF0\x90\x70\x80")); // Invalid third byte.
  assert (!utf8 ("\xF1\x80\x80\xC0")); // Invalid forth byte.

  // Out of the codepoint range (0x10ffff + 1).
  //
  assert (!utf8 ("\xF4\x90\x80\x80"));

  // Incomplete sequences.
  //
  assert (!utf8 ("\xD0"));         // 2-byte sequence.
  assert (!utf8 ("\xE4\xBA"));     // 3-byte sequence.
  assert (!utf8 ("\xF0\x90\x8C")); // 4-byte sequence.

  // Missing sequence leading bytes.
  //
  assert (!utf8 ("\xB0xyz"));         // 2-byte sequence.
  assert (!utf8 ("\xBA\x8Cxyz"));     // 3-byte sequence.
  assert (!utf8 ("\x8Cxyz"));         // 3-byte sequence.
  assert (!utf8 ("\x90\x8C\x82xyz")); // 4-byte sequence.
  assert (!utf8 ("\x8C\x82xyz"));     // 4-byte sequence.
  assert (!utf8 ("\x82xyz"));         // 4-byte sequence.

  // Whitelisting.
  //
  assert (utf8 ("\r\t\n"));

  // Matched codepoint types.
  //
  // Control.
  //
  assert (utf8 ("\r",   codepoint_types::control));
  assert (utf8 ("\x7F", codepoint_types::control));

  // Non-character.
  //
  assert (utf8 ("\xF4\x8F\xBF\xBF", codepoint_types::non_character));
  assert (utf8 ("\xEF\xB7\x90",     codepoint_types::non_character));

  // Private-use.
  //
  assert (utf8 ("\xEE\x80\x80",     codepoint_types::private_use));
  assert (utf8 ("\xF3\xB0\x80\x80", codepoint_types::private_use));

  // Reserved.
  //
  assert (utf8 ("\xF3\xA1\x80\x80", codepoint_types::reserved));
  assert (utf8 ("\xF0\xB0\x80\x80", codepoint_types::reserved));
  assert (utf8 ("\xF3\xA0\x82\x80", codepoint_types::reserved));

  // Format.
  //
  assert (utf8 ("\xC2\xAD",         codepoint_types::format));
  assert (utf8 ("\xD8\x80",         codepoint_types::format));
  assert (utf8 ("\xD8\x81",         codepoint_types::format));
  assert (utf8 ("\xD8\x85",         codepoint_types::format));
  assert (utf8 ("\xF3\xA0\x81\xBF", codepoint_types::format));

  // Graphic.
  //
  assert (utf8 ("\xC2\xAC",         codepoint_types::graphic));
  assert (utf8 ("\xC2\xAE",         codepoint_types::graphic));
  assert (utf8 ("\xD8\x86",         codepoint_types::graphic));
  assert (utf8 ("\xF3\xA0\x84\x80", codepoint_types::graphic));

  // Private-use & graphic.
  //
  assert (utf8 ("\xEE\x80\x80\xF3\xB0\x80\x80\xC2\xAC",
                codepoint_types::private_use | codepoint_types::graphic));

  // None.
  //
  assert (utf8 ("\t", codepoint_types::none, U"\t")); // Whitelisted.

  // Any.
  //
  assert (utf8 ("\t"));

  // Unmatched codepoint types.
  //
  assert (!utf8 ("\x7F", codepoint_types::graphic, U"\t"));      // Control.
  assert (!utf8 ("\xEF\xB7\x90", codepoint_types::graphic));     // Non-char.
  assert (!utf8 ("\xEE\x80\x80", codepoint_types::graphic));     // Private.
  assert (!utf8 ("\xF3\xA1\x80\x80", codepoint_types::graphic)); // Reserved.
  assert (!utf8 ("\xF3\xA0\x81\xBF", codepoint_types::graphic)); // Format.

  assert (!utf8 ("\xC2\xAC", codepoint_types::format)); // Graphic.

  // Private-use & Graphic.
  //
  assert (!utf8 ("\xEE\x80\x80\xF3\xB0\x80\x80\xC2\xAC",
                 codepoint_types::format));

  assert (!utf8 ("a", codepoint_types::none)); // None.
}
