// file      : libbutl/string-parser.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <vector>
#include <cstddef>   // size_t
#include <utility>   // pair
#include <stdexcept> // invalid_argument

#include <libbutl/export.hxx>

namespace butl
{
  namespace string_parser
  {
    class LIBBUTL_SYMEXPORT invalid_string: public std::invalid_argument
    {
    public:
      invalid_string (std::size_t p, const std::string& d)
        : invalid_argument (d), position (p) {}

      std::size_t position; // Zero-based.
    };

    // Parse a whitespace-separated list of strings. Can contain single or
    // double quoted substrings. No escaping is supported. If unquote is true,
    // return one-level unquoted values. Throw invalid_string in case of
    // invalid quoting.
    //
    LIBBUTL_SYMEXPORT std::vector<std::string>
    parse_quoted (const std::string&, bool unquote);

    // As above but return a list of string and zero-based position pairs.
    // Position is useful for issuing diagnostics about an invalid string
    // during second-level parsing.
    //
    LIBBUTL_SYMEXPORT std::vector<std::pair<std::string, std::size_t>>
    parse_quoted_position (const std::string&, bool unquote);

    // Remove a single level of quotes. Note that the format or the
    // correctness of the quotation is not validated.
    //
    LIBBUTL_SYMEXPORT std::string
    unquote (const std::string&);

    LIBBUTL_SYMEXPORT std::vector<std::string>
    unquote (const std::vector<std::string>&);
  }
}
