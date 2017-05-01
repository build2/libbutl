// file      : libbutl/utility.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef _WIN32
#  include <strings.h> // strcasecmp(), strncasecmp()
#else
#  include <string.h> // _stricmp(), _strnicmp()
#endif

#include <cctype> // toupper(), tolower(), isalpha(), isdigit(), isalnum()

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
  casecmp (char l, char r)
  {
    l = lcase (l);
    r = lcase (r);
    return l < r ? -1 : (l > r ? 1 : 0);
  }

  inline int
  casecmp (const char* l, const char* r, std::size_t n)
  {
#ifndef _WIN32
    return n == std::string::npos ? strcasecmp (l, r) : strncasecmp (l, r, n);
#else
    return n == std::string::npos ? _stricmp (l, r) : _strnicmp (l, r, n);
#endif
  }

  inline int
  casecmp (const std::string& l, const std::string& r, std::size_t n)
  {
    return casecmp (l.c_str (), r.c_str (), n);
  }

  inline int
  casecmp (const std::string& l, const char* r, std::size_t n)
  {
    return casecmp (l.c_str (), r, n);
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
}
