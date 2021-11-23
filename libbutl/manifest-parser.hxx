// file      : libbutl/manifest-parser.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <vector>
#include <iosfwd>
#include <cstdint>    // uint64_t
#include <utility>    // pair, move()
#include <stdexcept>  // runtime_error
#include <functional>

#include <libbutl/utf8.hxx>
#include <libbutl/optional.hxx>
#include <libbutl/char-scanner.hxx>
#include <libbutl/manifest-types.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  class LIBBUTL_SYMEXPORT manifest_parsing: public std::runtime_error
  {
  public:
    manifest_parsing (const std::string& name,
                      std::uint64_t line,
                      std::uint64_t column,
                      const std::string& description);

    manifest_parsing (const std::string& description);

    std::string name;
    std::uint64_t line;
    std::uint64_t column;
    std::string description;
  };

  class LIBBUTL_SYMEXPORT manifest_parser:
    protected char_scanner<utf8_validator, 2>
  {
  public:
    // The filter, if specified, is called by next() prior to returning the
    // pair to the caller. If the filter returns false, then the pair is
    // discarded.
    //
    // Note that the filter should handle the end-of-manifest pairs (see
    // below) carefully, so next() doesn't end up with an infinite cycle.
    //
    using filter_function = bool (manifest_name_value&);

    manifest_parser (std::istream& is,
                     const std::string& name,
                     std::function<filter_function> filter = {})
      : char_scanner (is,
                      utf8_validator (codepoint_types::graphic, U"\n\r\t")),
        name_ (name),
        filter_ (std::move (filter)) {}

    const std::string&
    name () const {return name_;}

    // The first returned pair is special "start-of-manifest" with empty name
    // and value being the format version: {"", "<ver>"}. After that we have a
    // sequence of ordinary pairs which are the manifest. At the end of the
    // manifest we have the special "end-of-manifest" pair with empty name and
    // value: {"", ""}. After that we can either get another start-of-manifest
    // pair (in which case the whole sequence repeats from the beginning) or
    // we get another end-of-manifest-like pair which signals the end of
    // stream (aka EOF) and which we will call the end-of-stream pair. To put
    // it another way, the parse sequence always has the following form:
    //
    // ({"", "<ver>"} {"<name>", "<value>"}* {"", ""})* {"", ""}
    //
    manifest_name_value
    next ();

    // Split the manifest value, optionally followed by ';' character and a
    // comment into the value/comment pair. Note that ';' characters in the
    // value must be escaped by the backslash.
    //
    static std::pair<std::string, std::string>
    split_comment (const std::string&);

  private:
    using base = char_scanner<utf8_validator, 2>;

    void
    parse_next (manifest_name_value&);

    void
    parse_name (manifest_name_value&);

    void
    parse_value (manifest_name_value&);

    // Skip spaces and return the first peeked non-space character and the
    // starting position of the line it belongs to. If the later is not
    // available (skipped spaces are all in the middle of a line, we are at
    // eos, etc.), then fallback to the first peeked character position.
    //
    std::pair<xchar, std::uint64_t>
    skip_spaces ();

    // As base::get() but in case of an invalid character throws
    // manifest_parsing.
    //
    xchar
    get (const char* what);

    // Get previously peeked character (faster).
    //
    void
    get (const xchar&);

    // As base::peek() but in case of an invalid character throws
    // manifest_parsing.
    //
    xchar
    peek (const char* what);

  private:
    const std::string name_;
    const std::function<filter_function> filter_;

    enum {start, body, end} s_ = start;
    std::string version_; // Current format version.

    // Buffer for a get()/peek() potential error.
    //
    std::string ebuf_;
  };

  // Parse and return a single manifest. Throw manifest_parsing in case of an
  // error.
  //
  // Note that the returned manifest doesn't contain the format version nor
  // the end-of-manifest/stream pairs.
  //
  LIBBUTL_SYMEXPORT std::vector<manifest_name_value>
  parse_manifest (manifest_parser&);

  // As above but append the manifest values to an existing list.
  //
  LIBBUTL_SYMEXPORT void
  parse_manifest (manifest_parser&, std::vector<manifest_name_value>&);

  // As above but return nullopt if eos is reached before reading any values.
  //
  LIBBUTL_SYMEXPORT optional<std::vector<manifest_name_value>>
  try_parse_manifest (manifest_parser&);

  // As above but append the manifest values to an existing list returning
  // false if eos is reached before reading any values.
  //
  LIBBUTL_SYMEXPORT bool
  try_parse_manifest (manifest_parser&, std::vector<manifest_name_value>&);
}

#include <libbutl/manifest-parser.ixx>
