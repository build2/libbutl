// file      : libbutl/uuid-io.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <iosfwd>

#include <libbutl/uuid.hxx>
#include <libbutl/export.hxx>

namespace butl
{
  // Insert lower case string representation.
  //
  LIBBUTL_SYMEXPORT std::ostream&
  operator<< (std::ostream&, const uuid&);

  // Extract string representation (lower or upper case).
  //
  LIBBUTL_SYMEXPORT std::istream&
  operator>> (std::istream&, uuid&);
}
