// file      : libbutl/utf8.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline utf8_validator::
  utf8_validator (codepoint_types ts, const char32_t* wl)
      : types_ (ts),
        whitelist_ (wl)
  {
  }

  inline std::pair<bool, bool> utf8_validator::
  validate (char c)
  {
    return validate (c, nullptr /* what */);
  }

  inline std::pair<bool, bool> utf8_validator::
  validate (char c, std::string& what)
  {
    return validate (c, &what);
  }

  inline std::pair<bool, bool> utf8_validator::
  validate (char c, std::string* what)
  {
    using namespace std;

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
    unsigned char b (c);

    // Compose the detailed "invalid UTF-8 sequence byte" error.
    //
    auto byte_error = [c, b, this] ()
    {
      string s ("invalid UTF-8 sequence ");

      const char* names[] = {"first", "second", "third", "forth"};
      s += names[seq_index_];
      s += " byte (0x";

      const char digits[] = "0123456789ABCDEF";
      s += digits[(b >> 4) & 0xF];
      s += digits[b & 0xF];

      // If the byte happens to be a printable ASCII character then let's
      // print it as a character as well. This can help a bit with grepping
      // through text while troubleshooting.
      //
      if (b >= 0x20 && b <= 0x7E)
      {
        s += " '";
        s += c;
        s += "'";
      }

      s += ")";
      return s;
    };

    // Detect the byte sequence length based on its first byte. While at it,
    // start calculating the resulting Unicode codepoint value.
    //
    if (seq_index_ == 0)
    {
      if (b < 0x80)
      {
        seq_size_ = 1;
        codepoint_ = b;
      }
      else if (b < 0xE0)
      {
        seq_size_ = 2;
        codepoint_ = b & 0x1F; // Takes 5 rightmost bits.
      }
      else if (b < 0xF0)
      {
        seq_size_ = 3;
        codepoint_ = b & 0xF; // Takes 4 rightmost bits.
      }
      else if (b < 0xF8)
      {
        seq_size_ = 4;
        codepoint_ = b & 0x7; // Takes 3 rightmost bits.
      }
      else
      {
        if (what != nullptr)
        {
          if (b < 0xFE)
          {
            *what  = b < 0xFC ? '5' : '6';
            *what += "-byte length UTF-8 sequence";
          }
          else
            *what = byte_error ();
        }

        return make_pair (false, false); // Invalid byte.
      }
    }

    // Note that while a codepoint may potentially be encoded with byte
    // sequences of different lengths, only the shortest encoding sequence is
    // considered well-formed. Also a well-formed sequence may not be decoded
    // into invalid codepoint value (see codepoint_type() for details). We
    // will check all that using the Well-Formed UTF-8 Byte Sequences table
    // (provided by the Unicode 12.0 Standard) which also takes care of the
    // missing UTF-8 sequence bytes.
    //
    bool valid (false);

    // Return true if a byte value belongs to the specified range.
    //
    auto belongs = [] (unsigned char c, unsigned char l, unsigned char r)
    {
      return c >= l && c <= r;
    };

    switch (seq_size_)
    {
    case 1: valid = true; break; // Well-formed by the definition (see above).
    case 2:
      {
        // [000080 0007FF]: [C2 DF]  [80 BF]
        //
        // Check the first byte and set the second byte range.
        //
        if (seq_index_ == 0)
        {
          if ((valid = belongs (b, 0xC2, 0xDF)))
            byte2_range_ = make_pair (0x80, 0xBF);
        }
        else // Check the second byte.
          valid = belongs (b, byte2_range_.first, byte2_range_.second);

        break;
      }
    case 3:
      {
        // [000800 000FFF]: E0       [A0 BF]  [80 BF]
        // [001000 00CFFF]: [E1 EC]  [80 BF]  [80 BF]
        // [00D000 00D7FF]: ED       [80 9F]  [80 BF] ; Excludes surrogates.
        // [00E000 00FFFF]: [EE EF]  [80 BF]  [80 BF]
        //
        // Check the first byte and set the second byte range.
        //
        if (seq_index_ == 0)
        {
          if ((valid = (b == 0xE0)))
            byte2_range_ = make_pair (0xA0, 0xBF);
          else if ((valid = belongs (b, 0xE1, 0xEC)))
            byte2_range_ = make_pair (0x80, 0xBF);
          else if ((valid = (b == 0xED)))
            byte2_range_ = make_pair (0x80, 0x9F);
          else if ((valid = belongs (b, 0xEE, 0xEF)))
            byte2_range_ = make_pair (0x80, 0xBF);
        }
        else if (seq_index_ == 1) // Check the second byte.
          valid = belongs (b, byte2_range_.first, byte2_range_.second);
        else                      // Check the third byte.
          valid = belongs (b, 0x80, 0xBF);

        break;
      }
    case 4:
      {
        // [010000 03FFFF]: F0       [90 BF]  [80 BF]  [80 BF]
        // [040000 0FFFFF]: [F1 F3]  [80 BF]  [80 BF]  [80 BF]
        // [100000 10FFFF]: F4       [80 8F]  [80 BF]  [80 BF]
        //
        // Check the first byte and set the second byte range.
        //
        if (seq_index_ == 0)
        {
          if ((valid = (b == 0xF0)))
            byte2_range_ = make_pair (0x90, 0xBF);
          else if ((valid = belongs (b, 0xF1, 0xF3)))
            byte2_range_ = make_pair (0x80, 0xBF);
          else if ((valid = (b == 0xF4)))
            byte2_range_ = make_pair (0x80, 0x8F);
        }
        else if (seq_index_ == 1) // Check the second byte.
          valid = belongs (b, byte2_range_.first, byte2_range_.second);
        else                      // Check the third and forth bytes.
          valid = belongs (b, 0x80, 0xBF);

        break;
      }
    }

    // Bail out if the current UTF-8 sequence byte is invalid.
    //
    if (!valid)
    {
      // We could probably distinguish "surrogate" and "exceed max value" from
      // other ill-formedness cases (amend the well-formedness table, keep
      // decoding the sequence, and test the codepoint in the end) and produce
      // more specific error messages, but this doesn't seem worth the
      // trouble.
      //
      if (what != nullptr)
        *what = byte_error ();

      return make_pair (false, false); // Invalid byte.
    }

    // "Append" the sequence byte's 6 rightmost bits to the resulting
    // codepoint value, unless this is the first byte (which value is already
    // taken into account; see above).
    //
    if (seq_index_ != 0)
      codepoint_ = (codepoint_ << 6) | (b & 0x3F);

    // If we didn't get to the end of the UTF-8 sequence, then we are done
    // with this byte.
    //
    if (++seq_index_ != seq_size_)
      return make_pair (true, false); // Valid byte.

    // Prepare for the next UTF-8 sequence validation, regardless of the
    // decoded codepoint validity.
    //
    seq_index_ = 0;

    // Check the decoded codepoint, unless any codepoint type is allowed.
    //
    // Note that the well-formedness sequence check guarantees that we decoded
    // a valid Unicode codepoint (see above).
    //
    if (types_ == codepoint_types::any)
      return make_pair (true, true); // Valid codepoint.

    // Check if the decoded codepoint is whitelisted.
    //
    using traits = u32string::traits_type;

    if (whitelist_ != nullptr &&
        traits::find (whitelist_, traits::length (whitelist_), codepoint_) !=
        nullptr)
      return make_pair (true, true); // Valid codepoint.

    // Now check if the codepoint type matches the specified set. Note: also
    // covers the `types_ == codepoint_types::none` case.
    //
    codepoint_types t (codepoint_type (codepoint_));

    if ((t & types_) != codepoint_types::none)
      return make_pair (true, true); // Valid codepoint.

    if (what != nullptr)
      *what = "invalid Unicode codepoint (" + to_string (t) + ")";

    return make_pair (false, true); // Invalid codepoint.
  }

  inline std::pair<bool, bool> utf8_validator::
  recover (char c)
  {
    // We are recovered if the character can be interpreted as a sequence
    // leading byte.
    //
    // As an optimization, bail out if the byte is a sequence trailing byte
    // (10xxxxxx).
    //
    if ((c & 0xC0) == 0x80)
      return std::make_pair (false, false); // Invalid byte.

    seq_index_ = 0;
    return validate (c);
  }

  inline char32_t utf8_validator::
  codepoint () const
  {
    return codepoint_;
  }
}
