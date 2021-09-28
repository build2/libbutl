// file      : libbutl/unicode.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/unicode.hxx>

#include <cstddef>   // size_t
#include <utility>   // pair
#include <algorithm> // lower_bound()

using namespace std;

namespace butl
{
  // Sorted arrays of the Unicode codepoint ranges corresponding to the
  // codepoint types (see the Types of Code Points table in the Unicode 12.0
  // Standard for details). Note that code type range lists (but not ranges
  // themselves) may overlap.
  //
  // Also note that the graphic type codepoints are numerous and scattered.
  // Thus, we will consider a codepoint to be of the graphic type if it is not
  // of any other type.
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

  // Return the codepoint type of a range if the codepoint value falls into
  // one and the graphic type otherwise.
  //
  // Note that this is a type detection fallback (see codepoint_type() for
  // details).
  //
  codepoint_types
  codepoint_type_lookup (char32_t c)
  {
    // Note that the codepoint type range lists may overlap. Thus, we iterate
    // over all of them until there is a match.
    //
    for (size_t i (0); i != sizeof (ct_ranges) / sizeof (*ct_ranges); ++i)
    {
      const codepoint_type_ranges& rs (ct_ranges[i]);

      // Find the range that either contains the codepoint or lays to the
      // right of it. Note that here we assume a range to be less than a
      // codepoint value if it lays to the left of the codepoint.
      //
      const codepoint_range* r (
        lower_bound (rs.begin, rs.end,
                     c,
                     [] (const codepoint_range& r, char32_t c)
                     {
                       return r.second < c;
                     }));

      if (r != rs.end && r->first <= c) // Contains the codepoint?
        return rs.type;
    }

    return codepoint_types::graphic;
  }
}
