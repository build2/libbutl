// file      : libbutl/utf8.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules_ts
#include <libbutl/utility.mxx>
#endif

#ifndef __cpp_lib_modules_ts
#include <string>
#include <cstddef>

#include <algorithm>    // lower_bound()
#endif

#ifdef __cpp_modules_ts
module butl.utility;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules_ts
import std.core;
import std.io;
#endif
#endif

#endif

namespace butl
{
  using namespace std;

  // Sorted arrays of the Unicode codepoint ranges corresponding to the
  // codepoint types. Note that code type range lists (but not ranges
  // themselves) may overlap.
  //
  // Note that the graphic type codepoints are numerous and scattered. Thus,
  // we will consider a codepoint to be of the graphic type if it is not of
  // any other type.
  //
  using codepoint_range = pair<char32_t, char32_t>;

  static const codepoint_range cn_rs[] = // Control.
  {
    {0x00, 0x1F},
    {0x7F, 0x9F}
  };

  static const codepoint_range fr_rs[] = // Format.
  {
    {0x000AD, 0x000AD},
    {0x00600, 0x00605},
    {0x0061C, 0x0061C},
    {0x006DD, 0x006DD},
    {0x0070F, 0x0070F},
    {0x008E2, 0x008E2},
    {0x0180E, 0x0180E},
    {0x0200B, 0x0200F},
    {0x0202A, 0x0202E},
    {0x02060, 0x02064},
    {0x02066, 0x0206F},
    {0x0FEFF, 0x0FEFF},
    {0x0FFF9, 0x0FFFB},
    {0x110BD, 0x110BD},
    {0x110CD, 0x110CD},
    {0x13430, 0x13438},
    {0x1BCA0, 0x1BCA3},
    {0x1D173, 0x1D17A},
    {0xE0001, 0xE0001},
    {0xE0020, 0xE007F}
  };

  static const codepoint_range pr_rs[] = // Private-use.
  {
    {0x00E000, 0x00F8FF},
    {0x0F0000, 0x10FFFF}
  };

  static const codepoint_range nc_rs[] = // Non-character.
  {
    {0xFDD0, 0xFDEF}
  };

  static const codepoint_range rs_rs[] = // Reserved.
  {
    {0x30000, 0xE0000},
    {0xE0002, 0xE001F},
    {0xE0080, 0xE00FF},
    {0xE01F0, 0xEFFFF}
  };

  struct codepoint_type_ranges
  {
    codepoint_types type;
    const codepoint_range* begin;
    const codepoint_range* end;
  };

  static const codepoint_type_ranges ct_ranges[] =
  {
    {
      codepoint_types::control,
      cn_rs,
      cn_rs + sizeof (cn_rs) / sizeof (*cn_rs)
    },
    {
      codepoint_types::format,
      fr_rs,
      fr_rs + sizeof (fr_rs) / sizeof (*fr_rs)
    },
    {
      codepoint_types::private_use,
      pr_rs,
      pr_rs + sizeof (pr_rs) / sizeof (*pr_rs)
    },
    {
      codepoint_types::non_character,
      nc_rs,
      nc_rs + sizeof (nc_rs) / sizeof (*nc_rs)
    },
    {
      codepoint_types::reserved,
      rs_rs,
      rs_rs + sizeof (rs_rs) / sizeof (*rs_rs)
    }
  };

  bool
  utf8 (const string& s, codepoint_types ts, const char32_t* wl)
  {
    // A UCS-4 character is encoded as the UTF-8 byte sequence as follows,
    // depending on the value range it falls into:
    //
    // 0x00000000 - 0x0000007F:
    //   0xxxxxxx
    //
    // 0x00000080 - 0x000007FF:
    //   110xxxxx 10xxxxxx
    //
    // 0x00000800 - 0x0000FFFF:
    //   1110xxxx 10xxxxxx 10xxxxxx
    //
    // 0x00010000 - 0x001FFFFF:
    //   11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    //
    // 0x00200000 - 0x03FFFFFF:
    //   111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
    //
    // 0x04000000 - 0x7FFFFFFF:
    //   1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
    //
    // Also note that the Unicode Standard (as of 12.1) specifies no
    // codepoints above 0x10FFFF, so we consider 5- and 6-byte length UTF-8
    // sequences as invalid (we could have added `unspecified` codepoint type
    // except that there are no UTF-8 validation tables defined for these
    // sequences).
    //
    size_t n (s.size ());

    for (size_t i (0); i != n; )
    {
      // Detect the UTF-8 byte sequence length based on its first byte. While
      // at it, start calculating the Unicode codepoint value.
      //
      size_t sn;
      char32_t c;
      unsigned char b1 (s[i]);

      if (b1 < 0x80)
      {
        sn = 1;
        c  = b1;
      }
      else if (b1 < 0xE0)
      {
        sn = 2;
        c  = b1 & 0x1F; // Takes 5 rightmost bits of the first sequence byte.
      }
      else if (b1 < 0xF0)
      {
        sn = 3;
        c  = b1 & 0xF; // Takes 4 rightmost bits of the first sequence byte.
      }
      else if (b1 < 0xF8)
      {
        sn = 4;
        c  = b1 & 0x7; // Takes 3 rightmost bits of the first sequence byte.
      }
      else
        return false; // The byte starts 5- or 6-byte length sequence.

      // Bail out if the string doesn't contain all the requred codepoint
      // encoding bytes.
      //
      if (sn > n - i)
        return false;

      // Note that while a codepoint may potentially be encoded with byte
      // sequences of different lengths, only the shortest encoding sequence
      // is considered well-formed. Also a well-formed sequence may not be
      // decoded into a UTF-16 surrogate value ([D800 DFFF]) or a value that
      // is greater than the max codepoint value (0x10FFFF). We will check all
      // that using the Well-Formed UTF-8 Byte Sequences table (provided by
      // the Unicode 12.0 Standard) which also takes care of the missing UTF-8
      // sequence bytes.
      //
      // Return true if a byte value belongs to the specified range.
      //
      auto belongs = [] (unsigned char c, unsigned char l, unsigned char r)
      {
        return c >= l && c <= r;
      };

      switch (sn)
      {
      case 1: break; // Always well-formed by the definition (see above).
      case 2:
        {
          // [000080 0007FF]: [C2 DF]  [80 BF]
          //
          // Check the first/second bytes combinations:
          //
          if (!(belongs (b1, 0xC2, 0xDF) && belongs (s[i + 1], 0x80, 0xBF)))
            return false;

          break;
        }
      case 3:
        {
          // [000800 000FFF]: E0       [A0 BF]  [80 BF]
          // [001000 00CFFF]: [E1 EC]  [80 BF]  [80 BF]
          // [00D000 00D7FF]: ED       [80 9F]  [80 BF] ; Excludes surrogates.
          // [00E000 00FFFF]: [EE EF]  [80 BF]  [80 BF]
          //
          unsigned char b2 (s[i + 1]);

          if (!((b1 == 0xE0               && belongs (b2, 0xA0, 0xBF)) ||
                (belongs (b1, 0xE1, 0xEC) && belongs (b2, 0x80, 0xBF)) ||
                (b1 == 0xED               && belongs (b2, 0x80, 0x9F)) ||
                (belongs (b1, 0xEE, 0xEF) && belongs (b2, 0x80, 0xBF))) ||
              !belongs (s[i + 2], 0x80, 0xBF))
            return false;

          break;
        }
      case 4:
        {
          // [010000 03FFFF]: F0       [90 BF]  [80 BF]  [80 BF]
          // [040000 0FFFFF]: [F1 F3]  [80 BF]  [80 BF]  [80 BF]
          // [100000 10FFFF]: F4       [80 8F]  [80 BF]  [80 BF]
          //
          unsigned char b2 (s[i + 1]);

          if (!((b1 == 0xF0               && belongs (b2, 0x90, 0xBF)) ||
                (belongs (b1, 0xF1, 0xF3) && belongs (b2, 0x80, 0xBF)) ||
                (b1 == 0xF4               && belongs (b2, 0x80, 0x8F))) ||
              !belongs (s[i + 2], 0x80, 0xBF)                           ||
              !belongs (s[i + 3], 0x80, 0xBF))
            return false;

          break;
        }
      }

      // For the remaining sequence bytes, "append" their 6 rightmost bits to
      // the resulting codepoint value.
      //
      --sn;
      ++i;

      for (size_t n (i + sn); i != n; ++i)
        c = (c << 6) | (s[i] & 0x3F);

      // Check the decoded codepoint, unless any codepoint type is allowed.
      //
      if (ts == codepoint_types::any)
        continue;

      using traits = u32string::traits_type;

      // Check if the decoded codepoint is whitelisted.
      //
      if (wl != nullptr &&
          traits::find (wl, traits::length (wl), c) != nullptr)
        continue;

      // Match the decoded codepoint type against the specified type set.
      //
      // Detect the codepoint type (see the Types of Code Points table in the
      // Unicode 12.0 Standard for details).
      //
      codepoint_types ct;

      // Optimize for the common case (printable ASCII characters).
      //
      if (c >= 0x20 && c <= 0x7E)
        ct = codepoint_types::graphic;
      else if ((c & 0xFFFF) >= 0xFFFE) // Non-range based detection.
        ct = codepoint_types::non_character;
      else
      {
        // Note that we consider a codepoint to be of the graphic type if it
        // is not of any other type (see above).
        //
        ct = codepoint_types::graphic;

        // Note that the codepoint type range lists may overlap. Thus, we
        // iterate over all of them until there is a match.
        //
        for (size_t i (0); i != sizeof (ct_ranges) / sizeof (*ct_ranges); ++i)
        {
          const codepoint_type_ranges& rs (ct_ranges[i]);

          // Find the range that either contains the codepoint or lays to the
          // right of it. Note that here we assume a range to be less than a
          // codepoint if it lays to the left of the codepoint.
          //
          const codepoint_range* r (
            lower_bound (rs.begin, rs.end,
                         c,
                         [] (const codepoint_range& r, char32_t c)
                         {
                           return r.second < c;
                         }));

          if (r != rs.end && r->first <= c) // Contains the codepoint?
          {
            ct = rs.type;
            break;
          }
        }
      }

      // Now check if the codepoint type matches the specified set. Note: also
      // covers the `ts == codepoint_types::none` case.
      //
      if ((ct & ts) == codepoint_types::none)
        return false;
    }

    return true;
  }
}
