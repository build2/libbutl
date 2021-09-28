// file      : libbutl/tab-parser.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <iosfwd>
#include <string>
#include <vector>
#include <cstdint>   // uint64_t
#include <stdexcept> // runtime_error

#include <libbutl/export.hxx>

namespace butl
{
  class LIBBUTL_SYMEXPORT tab_parsing: public std::runtime_error
  {
  public:
    tab_parsing (const std::string& name,
                 std::uint64_t line,
                 std::uint64_t column,
                 const std::string& description);

    std::string name;
    std::uint64_t line;
    std::uint64_t column;
    std::string description;
  };

  // Line and columns are useful for issuing diagnostics about invalid or
  // missing fields.
  //
  struct tab_field
  {
    std::string value;    // Field string (quoting preserved).
    std::uint64_t column; // Field start column number (one-based).
  };

  struct tab_fields: std::vector<tab_field>
  {
    std::uint64_t line;       // Line number (one-based).
    std::uint64_t end_column; // End-of-line column (line length).
  };

  // Read and parse lines consisting of space-separated fields. Field can
  // contain single or double quoted substrings (with spaces) which are
  // interpreted but preserved. No escaping of the quote characters is
  // supported. Blank lines and lines that start with # (collectively called
  // empty lines) are ignored.
  //
  class LIBBUTL_SYMEXPORT tab_parser
  {
  public:
    tab_parser (std::istream& is, const std::string& name)
        : is_ (is), name_ (name) {}

    // Return next line of fields. Skip empty lines. Empty result denotes the
    // end of stream.
    //
    tab_fields
    next ();

  private:
    std::istream& is_;
    const std::string name_;
    std::uint64_t line_ = 0;
  };
}
