// file      : libbutl/utility.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules
#include <libbutl/utility.mxx>
#endif

#ifdef _WIN32
#include <libbutl/win32-utility.hxx>
#endif

#include <stdlib.h> // setenv(), unsetenv(), _putenv()

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

  string&
  trim (string& l)
  {
    /*
    assert (trim (r = "") == "");
    assert (trim (r = " ") == "");
    assert (trim (r = " \t\r") == "");
    assert (trim (r = "a") == "a");
    assert (trim (r = " a") == "a");
    assert (trim (r = "a ") == "a");
    assert (trim (r = " \ta") == "a");
    assert (trim (r = "a \r") == "a");
    assert (trim (r = " a ") == "a");
    assert (trim (r = " \ta \r") == "a");
    assert (trim (r = "\na\n") == "a");
    */

    auto ws = [] (char c )
    {
      return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };

    size_t i (0), n (l.size ());

    for (; i != n && ws (l[i]);     ++i) ;
    for (; n != i && ws (l[n - 1]); --n) ;

    if (i != 0)
    {
      string s (l, i, n - i);
      l.swap (s);
    }
    else if (n != l.size ())
      l.resize (n);

    return l;
  }

  void
  setenv (const string& name, const string& value)
  {
#ifndef _WIN32
    if (::setenv (name.c_str (), value.c_str (), 1 /* overwrite */) == -1)
      throw_generic_error (errno);
#else
    // The documentation doesn't say how to obtain the failure reason, so we
    // will assume it to always be EINVAL (as the most probable one).
    //
    if (_putenv (string (name + '=' + value).c_str ()) == -1)
      throw_generic_error (EINVAL);
#endif
  }

  void
  unsetenv (const string& name)
  {
#ifndef _WIN32
    if (::unsetenv (name.c_str ()) == -1)
      throw_generic_error (errno);
#else
    // The documentation doesn't say how to obtain the failure reason, so we
    // will assume it to always be EINVAL (as the most probable one).
    //
    if (_putenv (string (name + '=').c_str ()) == -1)
      throw_generic_error (EINVAL);
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
    // ERROR_INVALID_DATA error code the original description will be 'Invalid
    // data. : Success' or 'Invalid data. : No error' for MinGW libstdc++ and
    // 'Invalid data. : Success.' or ". : The operation completed
    // successfully." for msvcrt.
    //
    // Check if the string ends with the specified suffix and return its
    // length if that's the case. So can be used as bool.
    //
    auto suffix = [s, n] (const char* v) -> size_t
    {
      size_t nv (string::traits_type::length (v));
      return (n >= nv && string::traits_type::compare (s + n - nv, v, nv) == 0
              ? nv
              : 0);
    };

    size_t ns;
    if ((ns = suffix (". : Success"))  ||
        (ns = suffix (". : No error")) ||
        (ns = suffix (". : The operation completed successfully")))
      n -= ns;

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
