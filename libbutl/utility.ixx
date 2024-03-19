// file      : libbutl/utility.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <cctype>    // toupper(), tolower(), is*()
#include <cwctype>   // isw*()
#include <algorithm> // for_each()
#include <stdexcept> // invalid_argument

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
  ucase (std::string& s, std::size_t p, std::size_t n)
  {
    if (n == std::string::npos)
      n = s.size () - p;

    if (n != 0)
    {
      s.front () = s.front (); // Force copy in CoW.
      ucase (const_cast<char*> (s.data ()) + p, n);
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
  ucase (const std::string& s, std::size_t p, std::size_t n)
  {
    return ucase (s.c_str () + p, n != std::string::npos ? n : s.size () - p);
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
  lcase (std::string& s, std::size_t p, std::size_t n)
  {
    if (n == std::string::npos)
      n = s.size () - p;

    if (n != 0)
    {
      s.front () = s.front (); // Force copy in CoW.
      lcase (const_cast<char*> (s.data ()) + p, n);
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
  lcase (const std::string& s, std::size_t p, std::size_t n)
  {
    return lcase (s.c_str () + p, n != std::string::npos ? n : s.size () - p);
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
  wspace (char c)
  {
    return std::isspace (c);
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

  inline bool
  wspace (wchar_t c)
  {
    return std::iswspace (c);
  }

  inline std::size_t
  next_word (const std::string& s, std::size_t& b, std::size_t& e,
             char d1, char d2)
  {
    return next_word (s, s.size (), b, e, d1, d2);
  }

  inline std::size_t
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

  inline std::size_t
  next_word (const std::string& s,
             std::size_t n, std::size_t& b, std::size_t& e, std::size_t& m,
             char d1, char d2)
  {
    // An empty word will necessarily be represented as b and e being the
    // position of a delimiter. Consider these corner cases (in all three we
    // should produce two words):
    //
    // \n
    // a\n
    // \na
    //
    // It feels sensible to represent an empty word as the position of the
    // trailing delimiter except if it is the last character (the first two
    // cases). Thus the additional m state, which, if 0 or 1 indicates the
    // number of delimiters to skip before parsing the next word and 2 if
    // this is a trailing delimiter for which we need to fake an empty word
    // with the leading delimiter.

    if (b != e)
      b = e;

    if (m > 1)
    {
      --m;
      return 0;
    }

    // Skip the leading delimiter, if any.
    //
    b += m;

    if (b == n)
    {
      e = n;
      return 0;
    }

    // Find first trailing delimiter.
    //
    m = 0;
    for (e = b; e != n; ++e)
    {
      if (s[e] == d1 || s[e] == d2)
      {
        m = 1;

        // Handle the special delimiter as the last character case.
        //
        if (e + 1 == n)
          ++m;

        break;
      }
    }

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

  inline void
  sanitize_strlit (const std::string& s, std::string& o)
  {
    for (std::size_t i (0), j;; i = j + 1)
    {
      j = s.find_first_of ("\\\"\n", i);
      o.append (s.c_str () + i, (j == std::string::npos ? s.size () : j) - i);

      if (j == std::string::npos)
        break;

      switch (s[j])
      {
      case '\\': o += "\\\\"; break;
      case '"':  o += "\\\""; break;
      case '\n': o += "\\n";  break;
      }
    }
  }

  inline std::string
  sanitize_strlit (const std::string& s)
  {
    std::string r;
    sanitize_strlit (s, r);
    return r;
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

  inline optional<std::size_t>
  utf8_length_impl (const std::string& s,
                    std::string* what,
                    codepoint_types ts,
                    const char32_t* wl)
  {
    using namespace std;

    // Optimize for an empty string.
    //
    if (s.empty ())
      return 0;

    size_t r (0);
    pair<bool, bool> v;
    utf8_validator val (ts, wl);

    for (char c: s)
    {
      v = val.validate (c, what);

      if (!v.first) // Invalid byte?
        return nullopt;

      if (v.second) // Last byte in the sequence?
        ++r;
    }

    // Make sure that the last UTF-8 sequence is complete.
    //
    if (!v.second)
    {
      if (what != nullptr)
        *what = "incomplete UTF-8 sequence";

      return nullopt;
    }

    return r;
  }

  inline std::size_t
  utf8_length (const std::string& s, codepoint_types ts, const char32_t* wl)
  {
    using namespace std;

    string what;
    if (optional<size_t> r = utf8_length_impl (s, &what, ts, wl))
      return *r;

    throw invalid_argument (what);
  }

  inline bool
  utf8 (const std::string& s,
        std::string& what,
        codepoint_types ts,
        const char32_t* wl)
  {
    return utf8_length_impl (s, &what, ts, wl).has_value ();
  }

  inline bool
  utf8 (const std::string& s, codepoint_types ts, const char32_t* wl)
  {
    return utf8_length_impl (s, nullptr, ts, wl).has_value ();
  }

#ifndef _WIN32
  inline const char* const*
  thread_env ()
  {
    return thread_env_;
  }

  inline void
  thread_env (const char* const* v)
  {
    thread_env_ = v;
  }
#endif

  // auto_thread_env
  //
  inline auto_thread_env::
  auto_thread_env (const char* const* new_env)
  {
    const char* const* cur_env (thread_env ());

    if (cur_env != new_env)
    {
      prev_env = cur_env;
      thread_env (new_env);
    }
  }

  inline auto_thread_env::
  auto_thread_env (auto_thread_env&& x) noexcept
      : prev_env (std::move (x.prev_env))
  {
    x.prev_env = nullopt;
  }

  inline auto_thread_env& auto_thread_env::
  operator= (auto_thread_env&& x) noexcept
  {
    if (this != &x)
    {
      prev_env = std::move (x.prev_env);
      x.prev_env = nullopt;
    }

    return *this;
  }

  inline auto_thread_env::
  ~auto_thread_env ()
  {
    if (prev_env)
      thread_env (*prev_env);
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
