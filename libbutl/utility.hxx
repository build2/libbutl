// file      : libbutl/utility.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_UTILITY_HXX
#define LIBBUTL_UTILITY_HXX

#include <string>
#include <iosfwd>                   // ostream
#include <cstddef>                  // size_t
#include <utility>                  // move(), forward()
#include <cstring>                  // strcmp(), strlen()
#include <libbutl/ft/exception.hxx> // uncaught_exceptions
#include <exception>                // exception, uncaught_exception(s)()
#include <libbutl/ft/lang.hxx>      // thread_local

//#include <functional> // hash

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
  std::string ucase (const char*, std::size_t = std::string::npos);
  std::string ucase (const std::string&);
  std::string& ucase (std::string&);
  void ucase (char*, std::size_t);

  char lcase (char);
  std::string lcase (const char*, std::size_t = std::string::npos);
  std::string lcase (const std::string&);
  std::string& lcase (std::string&);
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
  int casecmp (char, char);

  int casecmp (const std::string&, const std::string&,
               std::size_t = std::string::npos);

  int casecmp (const std::string&, const char*,
               std::size_t = std::string::npos);

  int casecmp (const char*, const char*, std::size_t = std::string::npos);

  // Case-insensitive key comparators (i.e., to be used in sets, maps, etc).
  //
  struct case_compare_string
  {
    bool operator() (const std::string& x, const std::string& y) const
    {
      return casecmp (x, y) < 0;
    }
  };

  struct case_compare_c_string
  {
    bool operator() (const char* x, const char* y) const
    {
      return casecmp (x, y) < 0;
    }
  };

  bool
  alpha (char);

  bool
  digit (char);

  bool
  alnum (char);

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
    bool operator() (const P& x, const P& y) const {return *x < *y;}
  };

  //struct hash_pointer_target
  //{
  //  template <typename P>
  //  std::size_t operator() (const P& x) const {return std::hash (*x);}
  //};

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

  // Call a function if there is an exception.
  //

  template <typename F>
  struct exception_guard;

  template <typename F>
  inline exception_guard<F>
  make_exception_guard (F f)
  {
    return exception_guard<F> (std::move (f));
  }

#ifdef __cpp_lib_uncaught_exceptions
  template <typename F>
  struct exception_guard
  {
    exception_guard (F f)
        : f_ (std::move (f)),
          u_ (std::uncaught_exceptions ()) {}

    ~exception_guard ()
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
  bool exception_unwinding_dtor_;

  // On Windows one cannot export a thread-local variable so we have to
  // use a wrapper functions.
  //
#ifdef _WIN32
  LIBBUTL_SYMEXPORT bool&
  exception_unwinding_dtor ();
#else
  inline bool&
  exception_unwinding_dtor () {return exception_unwinding_dtor_;}
#endif

  template <typename F>
  struct exception_guard
  {
    exception_guard (F f): f_ (std::move (f)) {}
    ~exception_guard ()
    {
      if (std::uncaught_exception ())
      {
        exception_unwinding_dtor () = true;
        f_ ();
        exception_unwinding_dtor () = false;
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

#endif // LIBBUTL_UTILITY_HXX
