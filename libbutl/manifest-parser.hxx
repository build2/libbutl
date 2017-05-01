// file      : libbutl/manifest-parser.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_MANIFEST_PARSER_HXX
#define LIBBUTL_MANIFEST_PARSER_HXX

#include <string>
#include <iosfwd>
#include <cstdint>   // uint64_t
#include <stdexcept> // runtime_error

#include <libbutl/export.hxx>

#include <libbutl/char-scanner.hxx>

namespace butl
{
  class LIBBUTL_EXPORT manifest_parsing: public std::runtime_error
  {
  public:
    manifest_parsing (const std::string& name,
                      std::uint64_t line,
                      std::uint64_t column,
                      const std::string& description);

    std::string name;
    std::uint64_t line;
    std::uint64_t column;
    std::string description;
  };

  class manifest_name_value
  {
  public:
    std::string name;
    std::string value;

    std::uint64_t name_line;
    std::uint64_t name_column;

    std::uint64_t value_line;
    std::uint64_t value_column;

    bool
    empty () const {return name.empty () && value.empty ();}
  };

  class LIBBUTL_EXPORT manifest_parser: protected butl::char_scanner
  {
  public:
    manifest_parser (std::istream& is, const std::string& name)
        : char_scanner (is), name_ (name) {}

    const std::string&
    name () const {return name_;}

    // The first returned pair is special "start-of-manifest" with
    // empty name and value being the format version: {"", "<ver>"}.
    // After that we have a sequence of ordinary pairs which are
    // the manifest. At the end of the manifest we have the special
    // "end-of-manifest" pair with empty name and value: {"", ""}.
    // After that we can either get another start-of-manifest pair
    // (in which case the whole sequence repeats from the beginning)
    // or we get another end-of-manifest pair which signals the end
    // of stream (aka EOF). To put it another way, the parse sequence
    // always has the following form:
    //
    // ({"", "<ver>"} {"<name>", "<value>"}* {"", ""})* {"", ""}
    //
    manifest_name_value
    next ();

  private:
    void
    parse_name (manifest_name_value&);

    void
    parse_value (manifest_name_value&);

    // Skip spaces and return the first peeked non-space character.
    //
    xchar
    skip_spaces ();

  private:
    const std::string name_;

    enum {start, body, end} s_ = start;
    std::string version_; // Current format version.
  };
}

#endif // LIBBUTL_MANIFEST_PARSER_HXX
