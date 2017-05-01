// file      : libbutl/win32-utility.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_WIN32_UTILITY_HXX
#define LIBBUTL_WIN32_UTILITY_HXX

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

#include <string>

#include <libbutl/export.hxx>

namespace butl
{
  namespace win32
  {
    LIBBUTL_EXPORT std::string
    error_msg (DWORD code);

    LIBBUTL_EXPORT std::string
    last_error_msg ();
  }
};

#endif // _WIN32

#endif // LIBBUTL_WIN32_UTILITY_HXX
