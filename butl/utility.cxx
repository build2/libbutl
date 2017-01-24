// file      : butl/utility.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <ostream>

#include <butl/utility>

namespace butl
{
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
