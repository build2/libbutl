// file      : libbutl/utility.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#ifndef _WIN32
#  include <strings.h> // strcasecmp(), strncasecmp()
#else
#  include <string.h> // _stricmp(), _strnicmp()
#endif

#include <string>
#include <iosfwd>       // ostream
#include <istream>
#include <cstddef>      // size_t
#include <utility>      // move(), forward(), pair
#include <cstring>      // strcmp(), strlen()
#include <exception>    // exception, uncaught_exception[s]()
//#include <functional> // hash

#include <libbutl/ft/lang.hxx>      // thread_local
#include <libbutl/ft/exception.hxx> // uncaught_exceptions

#include <libbutl/utf8.hxx>
#include <libbutl/unicode.hxx>
#include <libbutl/optional.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // Throw std::system_error with generic_category or system_category,
  // respectively.
  //
  // The generic version should be used for portable errno codes (those that
  // are mapped to std::errc). The system version should be used for platform-
  // specific codes, for example, additional errno codes on POSIX systems or
  // the result of GetLastError() on Windows.
  //
  // See also the exception sanitization below.
  //
  [[noreturn]] LIBBUTL_SYMEXPORT void
  throw_generic_error (int errno_code, const char* what = nullptr);

  [[noreturn]] LIBBUTL_SYMEXPORT void
  throw_system_error (int system_code, int fallback_errno_code = 0);

  // Throw std::ios::failure with the specified description and, if it is
  // derived from std::system_error (as it should), error code.
  //
  [[noreturn]] LIBBUTL_SYMEXPORT void
  throw_generic_ios_failure (int errno_code, const char* what = nullptr);

  [[noreturn]] LIBBUTL_SYMEXPORT void
  throw_system_ios_failure (int system_code, const char* what = nullptr);

  // Convert ASCII character/string case. If there is no upper/lower case
  // counterpart, leave the character unchanged. The POSIX locale (also known
  // as C locale) must be the current application locale. Otherwise the
  // behavior is undefined.
  //
  // Note that the POSIX locale specifies behaviour on data consisting
  // entirely of characters from the portable character set (subset of ASCII
  // including 103 non-negative characters and English alphabet letters in
  // particular) and the control character set (more about them at
  // http://pubs.opengroup.org/onlinepubs/009696899/basedefs/xbd_chap06.html).
  //
  // Also note that according to the POSIX locale definition the case
  // conversion can be applied only to [A-Z] and [a-z] character ranges being
  // translated to each other (more about that at
  // http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap07.html#tag_07_02)
  //
  char ucase (char);
  std::string ucase (const char*, std::size_t n = std::string::npos);
  std::string ucase (const std::string&,
                     std::size_t p = 0,
                     std::size_t n = std::string::npos);
  std::string& ucase (std::string&,
                      std::size_t p = 0,
                      std::size_t n = std::string::npos);
  void ucase (char*, std::size_t);

  char lcase (char);
  std::string lcase (const char*, std::size_t n = std::string::npos);
  std::string lcase (const std::string&,
                     std::size_t p = 0,
                     std::size_t n = std::string::npos);
  std::string& lcase (std::string&,
                      std::size_t p = 0,
                      std::size_t n = std::string::npos);
  void lcase (char*, std::size_t);

  // Compare ASCII characters/strings ignoring case. Behave as if characters
  // had been converted to the lower case and then byte-compared. Return a
  // negative, zero or positive value if the left hand side is less, equal or
  // greater than the right hand side, respectivelly. The POSIX locale (also
  // known as C locale) must be the current application locale. Otherwise the
  // behavior is undefined.
  //
  // The optional size argument specifies the maximum number of characters
  // to compare.
  //
  int icasecmp (char, char);

  int icasecmp (const std::string&, const std::string&,
                std::size_t = std::string::npos);

  int icasecmp (const std::string&, const char*,
                std::size_t = std::string::npos);

  int icasecmp (const char*, const char*, std::size_t = std::string::npos);

  // Case-insensitive key comparators (i.e., to be used in sets, maps, etc).
  //
  struct icase_compare_string
  {
    bool operator() (const std::string& x, const std::string& y) const
    {
      return icasecmp (x, y) < 0;
    }
  };

  struct icase_compare_c_string
  {
    bool operator() (const char* x, const char* y) const
    {
      return icasecmp (x, y) < 0;
    }
  };

  bool alpha  (char);
  bool digit  (char);
  bool alnum  (char);
  bool xdigit (char);
  bool wspace (char);

  bool alpha  (wchar_t);
  bool digit  (wchar_t);
  bool alnum  (wchar_t);
  bool xdigit (wchar_t);
  bool wspace (wchar_t);

  // Basic string utilities.
  //

  // Trim leading/trailing whitespaces, including '\n' and '\r'.
  //
  LIBBUTL_SYMEXPORT std::string&
  trim (std::string&);

  LIBBUTL_SYMEXPORT std::string&
  trim_left (std::string&);

  LIBBUTL_SYMEXPORT std::string&
  trim_right (std::string&);

  inline std::string
  trim (std::string&& s)
  {
    return move (trim (s));
  }

  inline std::string
  trim_left (std::string&& s)
  {
    return move (trim_left (s));
  }

  inline std::string
  trim_right (std::string&& s)
  {
    return move (trim_right (s));
  }

  // Find the beginning and end positions of the next word. Return the size
  // of the word or 0 and set b = e = n if there are no more words. For
  // example:
  //
  // for (size_t b (0), e (0); next_word (s, b, e); )
  // {
  //   string w (s, b, e - b);
  // }
  //
  // Or:
  //
  // for (size_t b (0), e (0), n; n = next_word (s, b, e, ' ', ','); )
  // {
  //   string w (s, b, n);
  // }
  //
  // The second version examines up to the n'th character in the string.
  //
  // The third version, instead of skipping consecutive delimiters, treats
  // them as separating empty words. The additional m variable contains an
  // unspecified internal state and should be initialized to 0. Note that in
  // this case you should use the (b == n) condition to detect the end. Note
  // also that a leading delimiter is considered as separating an empty word
  // from the rest and the trailing delimiter is considered as separating the
  // rest from an empty word. For example, this is how to parse lines while
  // observing blanks:
  //
  // for (size_t b (0), e (0), m (0), n (s.size ());
  //      next_word (s, n, b, e, m, '\n', '\r'), b != n; )
  // {
  //   string l (s, b, e - b);
  // }
  //
  // For string "\na\n" this code will observe the {"", "a", ""} words. And
  // for just "\n" it will observe the {"", ""} words.
  //
  std::size_t
  next_word (const std::string&, std::size_t& b, std::size_t& e,
             char d1 = ' ', char d2 = '\0');

  std::size_t
  next_word (const std::string&, std::size_t n, std::size_t& b, std::size_t& e,
             char d1 = ' ', char d2 = '\0');

  std::size_t
  next_word (const std::string&, std::size_t n,
             std::size_t& b, std::size_t& e, std::size_t& m,
             char d1 = ' ', char d2 = '\0');

  // Sanitize a string to only contain characters valid in an identifier
  // (ASCII alphanumeric plus `_`) replacing all others with `_`.
  //
  // Note that it doesn't make sure the first character is not a digit.
  //
  std::string& sanitize_identifier (std::string&);
  std::string  sanitize_identifier (std::string&&);
  std::string  sanitize_identifier (const std::string&);

  // Sanitize a string (e.g., a path) to be a valid C string literal by
  // escaping backslahes, double-quotes, and newlines.
  //
  // Note that in the second version the result is appended to out.
  //
  std::string sanitize_strlit (const std::string&);
  void        sanitize_strlit (const std::string&, std::string& out);

  // Return true if the string is a valid UTF-8 encoded byte string and,
  // optionally, its decoded codepoints belong to the specified types or
  // codepoint whitelist.
  //
  bool
  utf8 (const std::string&,
        codepoint_types = codepoint_types::any,
        const char32_t* whitelist = nullptr);

  // As above but in case of an invalid sequence also return the description
  // of why it is invalid.
  //
  bool
  utf8 (const std::string&,
        std::string& what,
        codepoint_types = codepoint_types::any,
        const char32_t* whitelist = nullptr);

  // Return UTF-8 byte string length in codepoints. Throw
  // std::invalid_argument if this is not a valid UTF-8.
  //
  std::size_t
  utf8_length (const std::string&,
               codepoint_types = codepoint_types::any,
               const char32_t* whitelist = nullptr);

  // Fixup the specified string (in place) to be valid UTF-8 replacing invalid
  // bytes and codepoints with the specified character, for example, '?'.
  //
  // Potential future improvements:
  //  - char32_t replacement (will need UTF-8 encoding)
  //  - different replacement for bytes and codepoints
  //
  LIBBUTL_SYMEXPORT void
  to_utf8 (std::string&,
           char replacement,
           codepoint_types = codepoint_types::any,
           const char32_t* whitelist = nullptr);

  // If an input stream is in a failed state, then return true if this is
  // because of the eof and throw istream::failure otherwise. If the stream
  // is not in a failed state, return false. This helper function is normally
  // used like this:
  //
  // is.exceptions (istream::badbit);
  //
  // for (string l; !eof (getline (is, l)); )
  // {
  //   ...
  // }
  //
  bool
  eof (std::istream&);

  // Environment variables.
  //
  // Our getenv() wrapper (as well as the relevant process startup functions)
  // have a notion of a "thread environment", that is, thread-specific
  // environment variables. However, unlike the process environment (in the
  // form of the environ array), the thread environment is specified as a set
  // of overrides over the process environment (sets and unsets), the same as
  // for the process startup.
  //
  // See also path_traits::thread_current_directory().
  //
  extern
#ifdef __cpp_thread_local
  thread_local
#else
  __thread
#endif
  const char* const* thread_env_;

  // On Windows one cannot export a thread-local variable so we have to
  // use wrapper functions.
  //
#ifdef _WIN32
  LIBBUTL_SYMEXPORT const char* const*
  thread_env ();

  LIBBUTL_SYMEXPORT void
  thread_env (const char* const*);
#else
  const char* const*
  thread_env ();

  void
  thread_env (const char* const*);
#endif

  struct auto_thread_env
  {
    optional<const char* const*> prev_env;

    auto_thread_env () = default;

    explicit
    auto_thread_env (const char* const*);

    // Move-to-empty-only type.
    //
    auto_thread_env (auto_thread_env&&) noexcept;
    auto_thread_env& operator= (auto_thread_env&&) noexcept;

    auto_thread_env (const auto_thread_env&) = delete;
    auto_thread_env& operator= (const auto_thread_env&) = delete;

    ~auto_thread_env ();
  };

  // Get the environment variables taking into account the current thread's
  // overrides (thread_env).
  //
  LIBBUTL_SYMEXPORT optional<std::string>
  getenv (const char*);

  inline optional<std::string>
  getenv (const std::string& n)
  {
    return getenv (n.c_str ());
  }

  // Set the process environment variable. Best done before starting any
  // threads (see thread_env). Throw system_error on failure.
  //
  // Note that on Windows setting an empty value unsets the variable.
  //
  LIBBUTL_SYMEXPORT void
  setenv (const std::string& name, const std::string& value);

  // Unset the process environment variable. Best done before starting any
  // threads (see thread_env). Throw system_error on failure.
  //
  LIBBUTL_SYMEXPORT void
  unsetenv (const std::string&);

  // Key comparators (i.e., to be used in sets, maps, etc).
  //
  struct compare_c_string
  {
    bool operator() (const char* x, const char* y) const noexcept
    {
      return std::strcmp (x, y) < 0;
    }
  };

  struct compare_pointer_target
  {
    template <typename P>
    bool operator() (const P& x, const P& y) const
    {
      return *x < *y;
    }
  };

  //struct hash_pointer_target
  //{
  //  template <typename P>
  //  std::size_t operator() (const P& x) const {return std::hash (*x);}
  //};

  // Compare two std::reference_wrapper's.
  //
  struct compare_reference_target
  {
    template <typename R>
    bool operator() (const R& x, const R& y) const
    {
      return x.get () < y.get ();
    }
  };

  // Combine one or more hash values.
  //
  inline std::size_t
  combine_hash (std::size_t s, std::size_t h)
  {
    // Magic formula from boost::hash_combine().
    //
    return s ^ (h + 0x9e3779b9 + (s << 6) + (s >> 2));
  }

  template <typename... S>
  inline std::size_t
  combine_hash (std::size_t s, std::size_t h, S... hs)
  {
    return combine_hash (combine_hash (s, h), hs...);
  }

  // Support for reverse iteration using range-based for-loop:
  //
  // for (... : reverse_iterate (x)) ...
  //
  template <typename T>
  class reverse_range
  {
    T x_;

  public:
    reverse_range (T&& x): x_ (std::forward<T> (x)) {}

    auto begin () const -> decltype (this->x_.rbegin ()) {return x_.rbegin ();}
    auto end () const -> decltype (this->x_.rend ()) {return x_.rend ();}
  };

  template <typename T>
  inline reverse_range<T>
  reverse_iterate (T&& x) {return reverse_range<T> (std::forward<T> (x));}

  // Cleanly cast between incompatible function types or dlsym() result
  // (void*) to a function pointer.
  //
  template <typename F, typename P>
  F
  function_cast (P*);

  // Call a function on destruction.
  //
  template <typename F>
  struct guard_impl;

  template <typename F>
  inline guard_impl<F>
  make_guard (F f)
  {
    return guard_impl<F> (std::move (f));
  }

  template <typename F>
  struct guard_impl
  {
    guard_impl (F f): function (std::move (f)), active (true) {}
    ~guard_impl () {if (active) function ();}

    void
    cancel () {active = false;}

    F function;
    bool active;
  };

  // Call a function if there is an exception.
  //

  template <typename F>
  struct exception_guard_impl;

  template <typename F>
  inline exception_guard_impl<F>
  make_exception_guard (F f)
  {
    return exception_guard_impl<F> (std::move (f));
  }

#ifdef __cpp_lib_uncaught_exceptions
  template <typename F>
  struct exception_guard_impl
  {
    exception_guard_impl (F f)
        : f_ (std::move (f)),
          u_ (std::uncaught_exceptions ()) {}

    ~exception_guard_impl ()
    {
      if (u_ != std::uncaught_exceptions ())
        f_ ();
    }

  private:
    F f_;
    int u_;
  };
#else
  // Fallback implementation using a TLS flag.
  //
  // True means we are in the body of a destructor that is being called as
  // part of the exception stack unwindining.
  //
  extern
#ifdef __cpp_thread_local
  thread_local
#else
  __thread
#endif
  // Work around glibc bug #14898.
  //
#if defined(__GLIBC__)       && \
    defined(__GLIBC_MINOR__) && \
    (__GLIBC__  < 2 || __GLIBC__ == 2 && __GLIBC_MINOR__ < 17)
  int
#else
  bool
#endif
  exception_unwinding_dtor_;

  // On Windows one cannot export a thread-local variable so we have to
  // use wrapper functions.
  //
#ifdef _WIN32
  LIBBUTL_SYMEXPORT bool
  exception_unwinding_dtor ();

  LIBBUTL_SYMEXPORT void
  exception_unwinding_dtor (bool);
#else
  inline bool
  exception_unwinding_dtor () {return exception_unwinding_dtor_;}

  inline void
  exception_unwinding_dtor (bool v) {exception_unwinding_dtor_ = v;}
#endif

  template <typename F>
  struct exception_guard_impl
  {
    exception_guard_impl (F f): f_ (std::move (f)) {}
    ~exception_guard_impl ()
    {
      if (std::uncaught_exception ())
      {
        exception_unwinding_dtor (true);
        f_ ();
        exception_unwinding_dtor (false);
      }
    }

  private:
    F f_;
  };
#endif
}

namespace std
{
  // Sanitize the exception description before printing. This includes:
  //
  // - stripping leading colons and spaces (see fdstream.cxx)
  // - stripping trailing newlines, periods, and spaces
  // - stripping system error redundant suffix (see utility.cxx)
  // - lower-case the first letter if the beginning looks like a word
  //
  LIBBUTL_SYMEXPORT ostream&
  operator<< (ostream&, const exception&);
}

#include <libbutl/utility.ixx>
