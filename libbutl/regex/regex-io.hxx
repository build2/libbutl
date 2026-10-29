#pragma once

#include <iosfwd>

#include <libstud/regex/regex.hxx>
#include <libstud/regex/export.hxx>

namespace stud
{
  namespace regex
  {
    // Print regex error description, converting the first character to lower
    // case and stripping the trailing dot.
    //
    LIBSTUD_REGEX_SYMEXPORT std::ostream&
    operator<< (std::ostream&, const regex_error&);
  }
}
