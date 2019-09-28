// file      : libbutl/utility.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_lib_modules_ts
#include <cstdlib> // getenv()
#include <algorithm>
#endif

namespace butl
{
  inline char
  ucase (char c)
  {
    return std::toupper (c);
  }

  inline void
  ucase (char* s, std::size_t n)
  {
    for (const char* e (s + n); s != e; ++s)
      *s = ucase (*s);
  }

  inline std::string&
  ucase (std::string& s)
  {
    if (size_t n = s.size ())
    {
      s.front () = s.front (); // Force copy in CoW.
      ucase (const_cast<char*> (s.data ()), n);
    }
    return s;
  }

  inline std::string
  ucase (const char* s, std::size_t n)
  {
    std::string r (s, n == std::string::npos ? std::strlen (s) : n);
    return ucase (r);
  }

  inline std::string
  ucase (const std::string& s)
  {
    return ucase (s.c_str (), s.size ());
  }

  inline char
  lcase (char c)
  {
    return std::tolower (c);
  }

  inline void
  lcase (char* s, std::size_t n)
  {
    for (const char* e (s + n); s != e; ++s)
      *s = lcase (*s);
  }

  inline std::string&
  lcase (std::string& s)
  {
    if (size_t n = s.size ())
    {
      s.front () = s.front (); // Force copy in CoW.
      lcase (const_cast<char*> (s.data ()), n);
    }
    return s;
  }

  inline std::string
  lcase (const char* s, std::size_t n)
  {
    std::string r (s, n == std::string::npos ? std::strlen (s) : n);
    return lcase (r);
  }

  inline std::string
  lcase (const std::string& s)
  {
    return lcase (s.c_str (), s.size ());
  }

  inline int
  icasecmp (char l, char r)
  {
    l = lcase (l);
    r = lcase (r);
    return l < r ? -1 : (l > r ? 1 : 0);
  }

  inline int
  icasecmp (const char* l, const char* r, std::size_t n)
  {
#ifndef _WIN32
    return n == std::string::npos ? strcasecmp (l, r) : strncasecmp (l, r, n);
#else
    return n == std::string::npos ? _stricmp (l, r) : _strnicmp (l, r, n);
#endif
  }

  inline int
  icasecmp (const std::string& l, const std::string& r, std::size_t n)
  {
    return icasecmp (l.c_str (), r.c_str (), n);
  }

  inline int
  icasecmp (const std::string& l, const char* r, std::size_t n)
  {
    return icasecmp (l.c_str (), r, n);
  }

  inline bool
  alpha (char c)
  {
    return std::isalpha (c);
  }

  inline bool
  digit (char c)
  {
    return std::isdigit (c);
  }

  inline bool
  alnum (char c)
  {
    return std::isalnum (c);
  }

  inline bool
  xdigit (char c)
  {
    return std::isxdigit (c);
  }

  inline bool
  alpha (wchar_t c)
  {
    return std::iswalpha (c);
  }

  inline bool
  digit (wchar_t c)
  {
    return std::iswdigit (c);
  }

  inline bool
  alnum (wchar_t c)
  {
    return std::iswalnum (c);
  }

  inline bool
  xdigit (wchar_t c)
  {
    return std::iswxdigit (c);
  }

  inline std::size_t
  next_word (const std::string& s, std::size_t& b, std::size_t& e,
             char d1, char d2)
  {
    return next_word (s, s.size (), b, e, d1, d2);
  }

  inline size_t
  next_word (const std::string& s,
             std::size_t n, std::size_t& b, std::size_t& e,
             char d1, char d2)
  {
    if (b != e)
      b = e;

    // Skip leading delimiters.
    //
    for (; b != n && (s[b] == d1 || s[b] == d2); ++b) ;

    if (b == n)
    {
      e = n;
      return 0;
    }

    // Find first trailing delimiter.
    //
    for (e = b + 1; e != n && s[e] != d1 && s[e] != d2; ++e) ;

    return e - b;
  }

  inline std::string&
  sanitize_identifier (std::string& s)
  {
    std::for_each (s.begin (), s.end (), [] (char& c)
                   {
                     if (!alnum (c) && c != '_')
                       c = '_';
                   });
    return s;
  }

  inline std::string
  sanitize_identifier (std::string&& s)
  {
    sanitize_identifier (s);
    return std::move (s);
  }

  inline std::string
  sanitize_identifier (const std::string& s)
  {
    return sanitize_identifier (std::string (s));
  }

  inline bool
  eof (std::istream& is)
  {
    if (!is.fail ())
      return false;

    if (is.eof ())
      return true;

    throw std::istream::failure ("");
  }

  inline optional<std::string>
  getenv (const std::string& name)
  {
    if (const char* r = std::getenv (name.c_str ()))
      return std::string (r);

    return nullopt;
  }

  template <typename F, typename P>
  inline F
  function_cast (P* p)
  {
    union { P* p; F f; } r;
    r.p = p;
    return r.f;
  }
}
