#pragma once

#include <iosfwd>

#include <libbutl/regex/regex.hxx>
#include <libbutl/export.hxx>

namespace butl
{
  namespace regex
  {
    // Print regex error description, converting the first character to lower
    // case and stripping the trailing dot.
    //
    LIBBUTL_SYMEXPORT std::ostream&
    operator<< (std::ostream&, const regex_error&);
  }
}
