// file      : tests/utf8/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <string>

#include <libbutl/utf8.hxx>
#include <libbutl/utility.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

int
main ()
{
  // utf8() tests.
  //
  auto utf8_error = [] (const string& s,
                        codepoint_types ts = codepoint_types::any,
                        const char32_t* wl = nullptr)
  {
    string error;
    assert (!utf8 (s, error, ts, wl));
    return error;
  };

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
  // Long sequences.
  //
  assert (!utf8 ("\xF8")); // 5-byte sequence.
  assert (!utf8 ("\xFC")); // 6-byte sequence.

  assert (utf8_error ("\xF8") == "5-byte length UTF-8 sequence");
  assert (utf8_error ("\xFC") == "6-byte length UTF-8 sequence");
  assert (utf8_error ("\xFE") == "invalid UTF-8 sequence first byte (0xFE)");

  // 2-byte sequences.
  //
  assert (!utf8 ("\xC1\x80")); // Invalid first byte.
  assert (!utf8 ("\xD0y"));    // Invalid second byte.

  assert (utf8_error ("\xC1\x80") ==
          "invalid UTF-8 sequence first byte (0xC1)");

  assert (utf8_error ("\xD0y") ==
          "invalid UTF-8 sequence second byte (0x79 'y')");

  // 3-byte sequences.
  //
  assert (!utf8 ("\xE2\x70\x80")); // Invalid second byte.
  assert (!utf8 ("\xE2\x80\x70")); // Invalid third byte.

  assert (!utf8 ("\xED\xA0\x80")); // Min UTF-16 surrogate.
  assert (!utf8 ("\xED\xBF\xBF")); // Max UTF-16 surrogate.

  assert (utf8_error ("\xE2\x80\x70") ==
          "invalid UTF-8 sequence third byte (0x70 'p')");

  // 4-byte sequences.
  //
  assert (!utf8 ("\xF5\x80\x80\x80")); // Invalid first byte.
  assert (!utf8 ("\xF0\x80\x80\x80")); // Invalid second byte.
  assert (!utf8 ("\xF0\x90\x70\x80")); // Invalid third byte.
  assert (!utf8 ("\xF1\x80\x80\xC0")); // Invalid forth byte.

  assert (utf8_error ("\xF1\x80\x80\xC0") ==
          "invalid UTF-8 sequence forth byte (0xC0)");

  // Incomplete sequences.
  //
  assert (!utf8 ("\xD0"));         // 2-byte sequence.
  assert (!utf8 ("\xE4\xBA"));     // 3-byte sequence.
  assert (!utf8 ("\xF0\x90\x8C")); // 4-byte sequence.

  assert (utf8_error ("\xD0") == "incomplete UTF-8 sequence");

  // Missing sequence leading bytes.
  //
  assert (!utf8 ("\xB0xyz"));            // 2-byte sequence.
  assert (!utf8 ("\xBA\x8C\xD0\xB0yz")); // 3-byte sequence.
  assert (!utf8 ("\x8Cxyz"));            // 3-byte sequence.
  assert (!utf8 ("\x90\x8C\x82xyz"));    // 4-byte sequence.
  assert (!utf8 ("\x8C\x82xyz"));        // 4-byte sequence.
  assert (!utf8 ("\x82xyz"));            // 4-byte sequence.

  assert (utf8_error ("\xB0") == "invalid UTF-8 sequence first byte (0xB0)");

  // Above the valid codepoint range (0x10ffff + 1).
  //
  assert (!utf8 ("\xF4\x90\x80\x80"));

  assert (utf8_error ("\xF4\x90\x80\x80") ==
          "invalid UTF-8 sequence second byte (0x90)");

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

  assert (utf8_error ("\xF3\xA0\x81\xBF", codepoint_types::graphic) ==
          "invalid Unicode codepoint (format)");

  assert (!utf8 ("\xC2\xAC", codepoint_types::format)); // Graphic.

  // Private-use & Graphic.
  //
  assert (!utf8 ("\xEE\x80\x80\xF3\xB0\x80\x80\xC2\xAC",
                 codepoint_types::format));

  assert (!utf8 ("a", codepoint_types::none)); // None.

  assert (utf8_error ("a", codepoint_types::none) ==
          "invalid Unicode codepoint (graphic)");

  // UTF-8 string length.
  //
  auto invalid_utf8 = [] (string s, codepoint_types ts = codepoint_types::any)
  {
    try
    {
      utf8_length (s, ts);
      return false;
    }
    catch (const invalid_argument&)
    {
      return true;
    }
  };

  assert (utf8_length ("") == 0);
  assert (utf8_length ("x\xD0\xB0\xE4\xBA\x8C\xF0\x90\x8C\x82y") == 5);

  assert (invalid_utf8 ("\xFE"));                         // Invalid byte.
  assert (invalid_utf8 ("\xD0"));                         // Incomplete.
  assert (invalid_utf8 ("\n", codepoint_types::graphic)); // Invalid codepoint.

  // to_utf8() tests.
  //
  auto roundtrip = [] (const char* s)
  {
    string r (s);
    to_utf8 (r, '?');
    return r == s;
  };

  auto sanitize = [] (string s, codepoint_types ts = codepoint_types::any)
  {
    to_utf8 (s, '?', ts);
    return s;
  };

  // Empty.
  //
  assert (roundtrip (""));

  // 1 code point.
  //
  assert (roundtrip ("a"));                // 1 byte.
  assert (roundtrip ("\xD0\xB0"));         // 2 bytes.
  assert (roundtrip ("\xE4\xBA\x8C"));     // 3 bytes.
  assert (roundtrip ("\xF0\x90\x8C\x82")); // 4 bytes.

  // Multiple code points.
  //
  assert (roundtrip ("a\xD0\xB0\xE4\xBA\x8C\xF0\x90\x8C\x82"));

  // Ill-formed sequences.
  //
  // Long sequence.
  //
  assert (sanitize ("\xF8") == "?"); // 5-byte sequence.

  // Invalid first byte followed by a second byte which ...
  //
  assert (sanitize ("\xC1\x80")     == "??");        // is a trailing byte.
  assert (sanitize ("\xC1y")        == "?y");        // starts 1-byte sequence.
  assert (sanitize ("\xC1\xD0\xB0") == "?\xD0\xB0"); // starts 2-byte sequence.
  assert (sanitize ("\xC1\xFE")     == "??");        // is not UTF-8.

  // Invalid second byte which ...
  //
  assert (sanitize ("\xD0y")        == "?y");        // starts 1-byte sequence.
  assert (sanitize ("\xD0\xD0\xB0") == "?\xD0\xB0"); // starts 2-byte sequence.
  assert (sanitize ("\xD0\xFE")     == "??");        // is not UTF-8.

  // Incomplete sequences.
  //
  assert (sanitize ("\xD0")     == "?");   // 2-byte sequence.
  assert (sanitize ("y\xD0")    == "y?");  // 2-byte sequence.
  assert (sanitize ("\xE4\xBA") == "??");  // 3-byte sequence.
  assert (sanitize ("\xD0\xD0") == "??");  // 2-byte sequence.

  // Incomplete recovery.
  //
  assert (sanitize ("\xD0\xFE")     == "??");  // 2-byte sequence.
  assert (sanitize ("\xD0\xFE\xFE") == "???"); // 2-byte sequence.

  assert (sanitize ("\xF4\x90\x80\x80") == "????"); // Above the range.
  assert (sanitize ("\xED\xA0\x80")     == "???");  // Min UTF-16 surrogate.
  assert (sanitize ("\xED\xBF\xBF")     == "???");  // Max UTF-16 surrogate.

  // Invalid codepoints.
  //
  auto sanitize_g = [&sanitize] (string s)
  {
    return sanitize (move (s), codepoint_types::graphic);
  };

  assert (sanitize_g ("\xEF\xB7\x90")  == "?");
  assert (sanitize_g ("y\xEF\xB7\x90") == "y?");
  assert (sanitize_g ("\xEF\xB7\x90y") == "?y");

  // Invalid during recovery.
  //
  assert (sanitize_g ("\xD0\n")     == "??");
  assert (sanitize_g ("\xD0\ny")    == "??y");
  assert (sanitize_g ("\xD0\xFE\n") == "???");

  assert (sanitize_g ("\xD0\xEF\xB7\x90") == "??");

  // utf8_validator::codepoint() tests.
  //
  {
    u32string r;
    size_t invalid_codepoints (0);

    string s ("a"
              "\xD0\xB0"
              "\n"                 // Control.
              "\xE4\xBA\x8C"
              "\xEE\x80\x80"       // Private-use.
              "\xF0\x90\x8C\x82");

    utf8_validator val (codepoint_types::graphic);

    for (char c: s)
    {
      pair<bool, bool> v (val.validate (c));

      if (v.first)
      {
        if (v.second)
          r.push_back (val.codepoint ());
      }
      else
        ++invalid_codepoints;
    }

    assert (r == U"a\x430\x4E8C\x10302");
    assert (invalid_codepoints == 2);
  }
}
