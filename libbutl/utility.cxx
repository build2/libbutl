// file      : libbutl/utility.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/utility.hxx>

#ifdef _WIN32
#include <libbutl/win32-utility.hxx>
#endif

#include <stdlib.h> // getenv(), setenv(), unsetenv(), _putenv()

#include <cstring>      // strncmp(), strlen()
#include <ostream>
#include <type_traits>  // enable_if, is_base_of
#include <system_error>

#include <libbutl/ft/lang.hxx>
#include <libbutl/ft/exception.hxx>

#include <libbutl/utf8.hxx>

namespace butl
{
  using namespace std;

#ifndef __cpp_lib_uncaught_exceptions

#ifdef __cpp_thread_local
  thread_local
#else
  __thread
#endif
#if defined(__GLIBC__)       && \
    defined(__GLIBC_MINOR__) && \
    (__GLIBC__  < 2 || __GLIBC__ == 2 && __GLIBC_MINOR__ < 17)
  int
#else
  bool
#endif
  exception_unwinding_dtor_ = false;

#ifdef _WIN32
  bool
  exception_unwinding_dtor () {return exception_unwinding_dtor_;}

  void
  exception_unwinding_dtor (bool v) {exception_unwinding_dtor_ = v;}
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

  // Throw ios::failure differently depending on whether it is derived from
  // system_error.
  //
  template <bool v>
  [[noreturn]] static inline typename enable_if<v>::type
  throw_ios_failure (error_code e, const char* w)
  {
    // The idea here is to make an error code to be saved into failure
    // exception and to make a string returned by what() to contain the error
    // description plus an optional custom message if provided. Unfortunatelly
    // there is no way to say that the custom message is absent. Passing an
    // empty string results for libstdc++ (as of version 5.3.1) with a
    // description like this (note the ': ' prefix):
    //
    // : No such file or directory
    //
    // Note that our custom operator<<(ostream, exception) strips this prefix.
    //
    throw ios_base::failure (w != nullptr ? w : "", e);
  }

  template <bool v>
  [[noreturn]] static inline typename enable_if<!v>::type
  throw_ios_failure (error_code e, const char* w)
  {
    throw ios_base::failure (w != nullptr ? w : e.message ().c_str ());
  }

  [[noreturn]] void
  throw_generic_ios_failure (int errno_code, const char* w)
  {
    error_code ec (errno_code, generic_category ());
    throw_ios_failure<is_base_of<system_error, ios_base::failure>::value> (
      ec, w);
  }

  [[noreturn]] void
  throw_system_ios_failure (int system_code, const char* w)
  {
#ifdef _WIN32
    // Here we work around MinGW libstdc++ that interprets Windows system error
    // codes (for example those returned by GetLastError()) as errno codes.
    //
    // Note that the resulting system_error description will have ': Success.'
    // suffix that is stripped by our custom operator<<(ostream, exception).
    //
    error_code ec (0, system_category ());
    string m;

    if (w == nullptr)
    {
      m = win32::error_msg (system_code);
      w = m.c_str ();
    }
#else
    error_code ec (system_code, system_category ());
#endif

    throw_ios_failure<is_base_of<system_error, ios_base::failure>::value> (
      ec, w);
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

    if (n != l.size ()) l.resize (n);
    if (i != 0)         l.erase (0, i);

    return l;
  }

  string&
  trim_left (string& l)
  {
    auto ws = [] (char c )
    {
      return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };

    size_t i (0), n (l.size ());

    for (; i != n && ws (l[i]); ++i) ;

    if (i != 0) l.erase (0, i);

    return l;
  }

  string&
  trim_right (string& l)
  {
    auto ws = [] (char c )
    {
      return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };

    size_t i (0), n (l.size ());

    for (; n != i && ws (l[n - 1]); --n) ;

    if (n != l.size ()) l.resize (n);

    return l;
  }

  void
  to_utf8 (string& s, char repl, codepoint_types ts, const char32_t* wl)
  {
    using iterator = string::iterator;

    utf8_validator val (ts, wl);

    iterator i (s.begin ()); // Source current position.
    iterator e (s.end ());   // Source end position.
    iterator d (i);          // Destination current position.
    iterator b (d);          // Begin of the current destination sequence.

    // Replace the current byte and prepare for the next sequence.
    //
    auto replace_byte = [&d, &b, repl] ()
    {
      *d++ = repl;
      b = d;
    };

    // Replace bytes of the current sequence excluding the current byte and
    // prepare for the next sequence.
    //
    auto replace_sequence = [&d, &b, repl] ()
    {
      for (; b != d; ++b)
        *b = repl;
    };

    // Replace sequence bytes with a single replacement byte and prepare for
    // the next sequence.
    //
    auto replace_codepoint = [&d, &b, &replace_byte] ()
    {
      d = b;           // Rewind to the beginning of the sequence.
      replace_byte ();
    };

    // Iterate over the byte string appending valid bytes, replacing invalid
    // bytes/codepoints, and recovering after invalid bytes.
    //
    for (; i != e; ++i)
    {
      char c (*i);
      pair<bool, bool> v (val.validate (c));

      // Append a valid byte and prepare for the next sequence if the sequence
      // end is reached.
      //
      auto append_byte = [&d, &b, &v, &c] ()
      {
        *d++ = c;

        if (v.second) // Sequence last byte?
          b = d;
      };

      // If this is a valid byte/codepoint, then append the byte and proceed
      // to the next string byte.
      //
      if (v.first)
      {
        append_byte ();
        continue;
      }

      // If this is an invalid codepoint, then replace the sequence with a
      // single replacement character and proceed to the next byte sequence
      // (no recovery is necessary).
      //
      if (v.second)
      {
        replace_codepoint ();
        continue;
      }

      // Now, given this is an invalid byte, replace the current sequence
      // bytes and recover.
      //
      replace_sequence ();

      // Stay in the recovery cycle until a valid byte is encountered. Note
      // that we start from where we left off, not from the next byte (see
      // utf8_validator::recover() for details).
      //
      for (; i != e; ++i)
      {
        c = *i;
        v = val.recover (c);

        // End the recovery cycle for a valid byte.
        //
        if (v.first)
        {
          append_byte ();
          break;
        }

        // End the recovery cycle for a decoded but invalid (ASCII-range)
        // codepoint.
        //
        if (v.second)
        {
          replace_codepoint ();
          break;
        }

        replace_byte ();
      }

      // Bail out if we reached the end of the byte string. Note that while we
      // failed to recover (otherwise i != e), all the bytes are already
      // replaced.
      //
      if (i == e)
        break;
    }

    // If the last byte sequence is incomplete, then replace its bytes.
    //
    if (b != d)
      replace_sequence ();

    // Shrink the byte string if we replaced any invalid codepoints.
    //
    if (d != e)
      s.resize (d - s.begin ());
  }

#ifdef __cpp_thread_local
  thread_local
#else
  __thread
#endif
  const char* const* thread_env_ = nullptr;

#ifdef _WIN32
  const char* const*
  thread_env () {return thread_env_;}

  void
  thread_env (const char* const* v) {thread_env_ = v;}
#endif

  optional<std::string>
  getenv (const char* name)
  {
    if (const char* const* vs = thread_env_)
    {
      size_t n (strlen (name));

      for (; *vs != nullptr; ++vs)
      {
        const char* v (*vs);

        // Note that on Windows variable names are case-insensitive.
        //
#ifdef _WIN32
        if (icasecmp (name, v, n) == 0)
#else
        if (strncmp (name, v, n) == 0)
#endif
        {
          switch (v[n])
          {
          case '=':  return string (v + n + 1);
          case '\0': return nullopt;
          }
        }
      }
    }

    if (const char* r = ::getenv (name))
      return std::string (r);

    return nullopt;
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
    for (; n != 0; --n)
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
    auto suffix = [s, &n] (const char* v) -> size_t
    {
      size_t nv (string::traits_type::length (v));
      return (n >= nv && string::traits_type::compare (s + n - nv, v, nv) == 0
              ? nv
              : 0);
    };

    // Note that in some cases we need to re-throw exception using a different
    // type, for example system_error as ios::failure. In such cases these
    // suffixed may accumulate, for example:
    //
    // Access denied. : Success.: Success.
    //
    // Thus, we will strip them in a loop.
    //
    size_t ns;
    while ((ns = suffix (": Success"))  ||
           (ns = suffix (": No error")) ||
           (ns = suffix (": The operation completed successfully")))
    {
      for (n -= ns; n != 0 && (s[n-1] == '.' || s[n-1] == ' '); --n) ;
    }

    // Lower-case the first letter if the beginning looks like a word (the
    // second character is the lower-case letter or space).
    //
    char c;
    bool lc (n != 0 && alpha (c = s[0]) && c == ucase (c) &&
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
