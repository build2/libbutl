// file      : libbutl/prompt.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>

#include <libbutl/export.hxx>

namespace butl
{
  // The Y/N prompt. The def argument, if specified, should be either 'y' or
  // 'n'. It is used as the default answer, in case the user just hits enter.
  //
  // Write the prompt to diag_stream. Throw ios_base::failure if no answer
  // could be extracted from stdin (for example, because it was closed).
  //
  LIBBUTL_SYMEXPORT bool
  yn_prompt (const std::string&, char def = '\0');
}
