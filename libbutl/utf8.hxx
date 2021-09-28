// file      : libbutl/utf8.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <cstdint> // uint8_t
#include <utility> // pair

#include <libbutl/unicode.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // Here and below we will refer to bytes that encode a singe Unicode
  // codepoint as "UTF-8 byte sequence" ("UTF-8 sequence" or "byte sequence"
  // for short) and a sequence of such sequences as "UTF-8 encoded byte
  // string" ("byte string" for short).
  //

  // Validate a UTF-8 encoded byte string one byte at a time. Optionally, also
  // validate that its decoded codepoints belong to the specified types or
  // codepoint whitelist.
  //
  class utf8_validator
  {
  public:
    // Note: use whitelist via shallow copy.
    //
    explicit
    utf8_validator (codepoint_types = codepoint_types::any,
                    const char32_t* whitelist = nullptr);

    // Validate the next byte returning true if it is valid (first) and
    // whether it is the last byte of a codepoint (second). The {false, true}
    // result indicates a byte sequence decoded into a codepoint of undesired
    // type rather than an invalid byte that happens to be the last in the
    // sequence (and may well be a valid starting byte of the next sequence).
    //
    // Note that in case the byte is invalid, calling this function again
    // without recovery is illegal.
    //
    std::pair<bool, bool>
    validate (char);

    // As above but in case of an invalid byte also return the description of
    // why it is invalid.
    //
    // Note that the description only contains the reason why the specified
    // byte is not part of a valid UTF-8 sequence or the desired codepoint
    // type, for example:
    //
    // "invalid UTF-8 sequence first byte (0xB0)"
    // "invalid Unicode codepoint (reserved)"
    //
    // It can be used to form complete diagnostics along these lines:
    //
    // cerr << "invalid manifest value " << name << ": " << what << endl;
    //
    std::pair<bool, bool>
    validate (char, std::string& what);

    // As above but decide whether the description is needed at runtime (what
    // may be NULL).
    //
    std::pair<bool, bool>
    validate (char, std::string* what);

    // Recover from an invalid byte.
    //
    // This function must be called with the first invalid and then subsequent
    // bytes until it signals that the specified byte is valid. Note that it
    // shall not be called if the sequence is decoded into a codepoint of an
    // undesired type.
    //
    // Note also that a byte being invalid in the middle of a UTF-8 sequence
    // may be valid as a first byte of the next sequence.
    //
    std::pair<bool, bool>
    recover (char);

    // Return the codepoint of the last byte sequence.
    //
    // This function can only be legally called after validate() or recover()
    // signal that the preceding byte is valid and last.
    //
    char32_t
    codepoint () const;

  private:
    codepoint_types types_;
    const char32_t* whitelist_;

    // State machine.
    //
    uint8_t seq_size_;      // [1 4]; calculated at the first byte validation.
    uint8_t seq_index_ = 0; // [0 3]

    // Last byte sequence decoded codepoint (built incrementally).
    //
    char32_t codepoint_;

    // The byte range a valid UTF-8 sequence second byte must belong to as
    // calculated during the first byte validation.
    //
    // Note that the subsequent (third and forth) bytes must belong to the
    // [80 BF] range regardless to the previous bytes.
    //
    std::pair<unsigned char, unsigned char> byte2_range_;
  };
}

#include <libbutl/utf8.ixx>
