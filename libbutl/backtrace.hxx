// file      : libbutl/backtrace.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>

#include <libbutl/export.hxx>

namespace butl
{
  // Return the calling thread's backtrace or empty string if this
  // functionality is not supported or an error has occurred. The exact
  // backtrace format is implementation-defined; it normally contains a line
  // with the binary name, address in that binary, and, if available, the
  // function name for each stack frame.
  //
  // Currently this functionality is only available on Linux (with glibc),
  // FreeBSD/NetBSD, and Mac OS. On the first two platforms the address
  // can be mapped to the function name and, if built with debug info, to
  // source location using the addr2line(1) utility:
  //
  // $ addr2line -f -C -e <binary> <addr>
  //
  LIBBUTL_SYMEXPORT std::string
  backtrace () noexcept;
}
