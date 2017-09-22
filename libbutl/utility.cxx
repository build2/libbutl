// file      : libbutl/utility.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules
#include <libbutl/utility.mxx>
#endif

#ifdef _WIN32
#include <libbutl/win32-utility.hxx>
#endif

#ifndef __cpp_lib_modules
#include <string>
#include <cstddef>
#include <utility>

#include <ostream>
#include <system_error>
#endif

#include <libbutl/ft/lang.hxx>
#include <libbutl/ft/exception.hxx>

#ifdef __cpp_modules
module butl.utility;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules
import std.core;
import std.io;
#endif
#endif

#endif


namespace butl
{
  using namespace std;

#ifndef __cpp_lib_uncaught_exceptions

#ifdef __cpp_thread_local
  thread_local
#else
  __thread
#endif
  bool exception_unwinding_dtor_ = false;

#ifdef _WIN32
  bool&
  exception_unwinding_dtor () {return exception_unwinding_dtor_;}
#endif

#endif

  [[noreturn]] void
  throw_generic_error (int errno_code, const char* what)
  {
    if (what == nullptr)
      throw system_error (errno_code, generic_category ());
    else
      throw system_error (errno_code, generic_category (), what);
  }

  [[noreturn]] void
#ifndef _WIN32
  throw_system_error (int system_code, int)
  {
    throw system_error (system_code, system_category ());
#else
  throw_system_error (int system_code, int fallback_errno_code)
  {
    // Here we work around MinGW libstdc++ that interprets Windows system error
    // codes (for example those returned by GetLastError()) as errno codes. The
    // resulting system_error description will have the following form:
    //
    // <system_code description>: <fallback_errno_code description>
    //
    // Also note that the fallback-related description suffix is stripped by
    // our custom operator<<(ostream, exception) for the common case (see
    // below).
    //
    throw system_error (fallback_errno_code,
                        system_category (),
                        win32::error_msg (system_code));
#endif
  }
}

namespace std
{
  using namespace butl;

  ostream&
  operator<< (ostream& o, const exception& e)
  {
    const char* d (e.what ());
    const char* s (d);

    // Strip the leading junk (colons and spaces).
    //
    // Note that error descriptions for ios_base::failure exceptions thrown by
    // fdstream can have the ': ' prefix for libstdc++ (read more in comment
    // for throw_ios_failure()).
    //
    for (; *s == ' ' || *s == ':'; ++s) ;

    // Strip the trailing junk (periods, spaces, newlines).
    //
    // Note that msvcrt adds some junk like this:
    //
    // Invalid data.\r\n
    //
    size_t n (string::traits_type::length (s));
    for (; n > 0; --n)
    {
      switch (s[n-1])
      {
      case '\r':
      case '\n':
      case '.':
      case ' ': continue;
      }

      break;
    }

    // Strip the suffix for system_error thrown by
    // throw_system_error(system_code) on Windows. For example for the
    // ERROR_INVALID_DATA error code the original description will be
    // 'Invalid data. : Success' for MinGW libstdc++ and
    // 'Invalid data. : Success.' for msvcrt.
    //
    if (n >= 11 &&
        string::traits_type::compare (s + n - 11, ". : Success", 11) == 0)
      n -= 11;

    // Lower-case the first letter if the beginning looks like a word (the
    // second character is the lower-case letter or space).
    //
    char c;
    bool lc (n > 0 && alpha (c = s[0]) && c == ucase (c) &&
             (n == 1 || (alpha (c = s[1]) && c == lcase (c)) || c == ' '));

    // Print the description as is if no adjustment is required.
    //
    if (!lc && s == d && s[n] == '\0')
      o << d;
    else
    {
      // We need to produce the resulting description and then write it
      // with a single formatted output operation.
      //
      string r (s, n);

      if (lc)
        r[0] = lcase (r[0]);

      o << r;
    }

    return o;
  }
}
