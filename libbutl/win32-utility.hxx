// file      : libbutl/win32-utility.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#pragma once

// Use this header to include <windows.h> and a couple of Win32-specific
// utilities.
//

#ifdef _WIN32

// Try to include <windows.h> so that it doesn't mess other things up.
//
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#  ifndef NOMINMAX // No min and max macros.
#    define NOMINMAX
#    include <windows.h>
#    undef NOMINMAX
#  else
#    include <windows.h>
#  endif
#  undef WIN32_LEAN_AND_MEAN
#else
#  ifndef NOMINMAX
#    define NOMINMAX
#    include <windows.h>
#    undef NOMINMAX
#  else
#    include <windows.h>
#  endif
#endif

#ifndef __cpp_lib_modules
#include <string>
#else
import std.core;
#endif

#include <libbutl/export.hxx>

namespace butl
{
  namespace win32
  {
    LIBBUTL_SYMEXPORT std::string
    error_msg (DWORD code);

    LIBBUTL_SYMEXPORT std::string
    last_error_msg ();
  }
};

#endif // _WIN32
