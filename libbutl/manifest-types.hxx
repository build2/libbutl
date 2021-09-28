// file      : libbutl/manifest-types.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <cstdint> // uint64_t

#include <libbutl/export.hxx>

namespace butl
{
  class manifest_name_value
  {
  public:
    std::string name;
    std::string value;

    std::uint64_t name_line;
    std::uint64_t name_column;

    std::uint64_t value_line;
    std::uint64_t value_column;

    std::uint64_t start_pos; // Position of name/value-starting character.
    std::uint64_t colon_pos; // Position of name/value-separating ':'.
    std::uint64_t   end_pos; // Position of name/value-terminating '\n' or EOF.

    bool
    empty () const {return name.empty () && value.empty ();}
  };
}
