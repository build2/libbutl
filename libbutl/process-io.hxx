// file      : libbutl/process-io.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <ostream>

#include <libbutl/process.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  inline std::ostream&
  operator<< (std::ostream& o, const process_path& p)
  {
    return o << p.recall_string ();
  }

  inline std::ostream&
  operator<< (std::ostream& o, const process_args& a)
  {
    process::print (o, a.argv, a.argc);
    return o;
  }

  // Print the environment variables and the current working directory (if
  // specified) in a POSIX shell command line notation. The process path
  // itself is not printed. For example:
  //
  // LC_ALL=C
  //
  // If an environment variable is in the `name` rather than in the
  // `name=value` form, then it is considered unset. Since there is no POSIX
  // way to unset a variable on the command line, this information is printed
  // as `name=` (ambiguous with assigning an empty value but the two cases are
  // normally handled in the same way). For example:
  //
  // PATH= LC_ALL=C
  //
  // Note that since there is no POSIX way to change the current working
  // directory of a command to be executed, this information is printed in a
  // pseudo-notation by assigning to PWD (which, according POSIX, would result
  // in the undefined behavior of the cwd utility). For example:
  //
  // PWD=/tmp LC_ALL=C
  //
  LIBBUTL_SYMEXPORT std::ostream&
  operator<< (std::ostream&, const process_env&);
}
