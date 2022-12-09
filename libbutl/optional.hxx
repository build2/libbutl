// file      : libbutl/optional.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

// Note: the Clang check must come before GCC since it also defines __GNUC__.
//
#if   defined(ODB_COMPILER)
   //
   // Make sure we use butl::optional during ODB compilation (has to be this
   // way until we completely switch to std::optional since we use the same
   // generated code for all compilers).
   //
#elif defined(_MSC_VER)
   //
   // Available from 19.10 (15.0). Except it (or the compiler) doesn't seem to
   // be constexpr-correct. Things appear to be fixed in 19.20 (16.0) but
   // optional is now only available in the C++17 mode or later.
   //
#  if _MSC_VER >= 1920
#    if defined(_MSVC_LANG) && _MSVC_LANG >= 201703L // See /Zc:__cplusplus
#      define LIBBUTL_STD_OPTIONAL
#    endif
#  endif
#elif defined(__clang__)
   //
   // Clang's libc++ has it since 4 but we might also be using libstdc++. For
   // the latter we will check for the presence of the <optional> header which
   // only appeared in GCC 7. Also assume both are only available in C++17.
   //
   // Note that on Mac OS it can still be <experimental/optional>.
   //
#  if __cplusplus >= 201703L
#    if __has_include(<__config>)
#      include <__config>          // _LIBCPP_VERSION
#      if _LIBCPP_VERSION >= 4000 && __has_include(<optional>)
#        define LIBBUTL_STD_OPTIONAL
#      endif
#    elif __has_include(<optional>)
#      define LIBBUTL_STD_OPTIONAL
#    endif
#  endif
#elif defined(__GNUC__)
   //
   // Available from 7 but only in the C++17 mode. Note also that from 8
   // <optional> defines __cpp_lib_optional.
   //
#  if __GNUC__ >= 7 && __cplusplus >= 201703L
#    define LIBBUTL_STD_OPTIONAL
#  endif
#endif

#ifdef LIBBUTL_STD_OPTIONAL
#  include <optional>
#else
#  include <utility>     // move()
#  include <functional>  // hash
#  include <type_traits> // is_*
#endif

#include <libbutl/export.hxx>

#ifdef LIBBUTL_STD_OPTIONAL
namespace butl
{
  using std::optional;
  using std::nullopt_t;
  using std::nullopt;
}
#else

namespace butl
{
  // Simple optional class template while waiting for std::optional.
  //
  struct nullopt_t {constexpr explicit nullopt_t (int) {}};
  constexpr nullopt_t nullopt (1);

  namespace details
  {
    template <typename T, bool = std::is_trivially_destructible<T>::value>
    struct optional_data;

    template <typename T>
    struct optional_data<T, false>
    {
      struct empty {};

      union
      {
        empty e_;
        T d_;
      };
      bool v_;

#if !defined(_MSC_VER) || _MSC_VER > 1900
      constexpr optional_data ():           e_ (),              v_ (false) {}
      constexpr optional_data (nullopt_t):  e_ (),              v_ (false) {}
      constexpr optional_data (const T& v): d_ (v),             v_ (true)  {}
      constexpr optional_data (T&& v):      d_ (std::move (v)), v_ (true)  {}
#else
      optional_data ():           e_ (),              v_ (false) {}
      optional_data (nullopt_t):  e_ (),              v_ (false) {}
      optional_data (const T& v): d_ (v),             v_ (true)  {}
      optional_data (T&& v):      d_ (std::move (v)), v_ (true)  {}
#endif

#if (!defined(_MSC_VER) || _MSC_VER > 1900) &&  \
    (!defined(__GNUC__) || __GNUC__ > 4 || defined(__clang__))
      constexpr optional_data (const optional_data& o): v_ (o.v_) {if (v_) new (&d_) T (o.d_);}

      constexpr optional_data (optional_data&& o)
        noexcept (std::is_nothrow_move_constructible<T>::value)
        : v_ (o.v_) {if (v_) new (&d_) T (std::move (o.d_));}
#else
      optional_data (const optional_data& o): v_ (o.v_) {if (v_) new (&d_) T (o.d_);}

      optional_data (optional_data&& o)
        noexcept (std::is_nothrow_move_constructible<T>::value)
        : v_ (o.v_) {if (v_) new (&d_) T (std::move (o.d_));}
#endif

      optional_data& operator= (nullopt_t);
      optional_data& operator= (const T&);
      optional_data& operator= (T&&);

      optional_data& operator= (const optional_data&);

      optional_data& operator= (optional_data&&)
        noexcept (std::is_nothrow_move_constructible<T>::value &&
                  std::is_nothrow_move_assignable<T>::value    &&
                  std::is_nothrow_destructible<T>::value);

      ~optional_data ();
    };

    template <typename T>
    struct optional_data<T, true>
    {
      struct empty {};

      union
      {
        empty e_;
        T d_;
      };
      bool v_;

#if !defined(_MSC_VER) || _MSC_VER > 1900
      constexpr optional_data ():           e_ (),              v_ (false) {}
      constexpr optional_data (nullopt_t):  e_ (),              v_ (false) {}
      constexpr optional_data (const T& v): d_ (v),             v_ (true)  {}
      constexpr optional_data (T&& v):      d_ (std::move (v)), v_ (true)  {}
#else
      optional_data ():           e_ (),              v_ (false) {}
      optional_data (nullopt_t):  e_ (),              v_ (false) {}
      optional_data (const T& v): d_ (v),             v_ (true)  {}
      optional_data (T&& v):      d_ (std::move (v)), v_ (true)  {}
#endif

#if (!defined(_MSC_VER) || _MSC_VER > 1900) && \
    (!defined(__GNUC__) || __GNUC__ > 4 || defined(__clang__))
      constexpr optional_data (const optional_data& o): v_ (o.v_) {if (v_) new (&d_) T (o.d_);}

      constexpr optional_data (optional_data&& o)
        noexcept (std::is_nothrow_move_constructible<T>::value)
        : v_ (o.v_) {if (v_) new (&d_) T (std::move (o.d_));}
#else
      optional_data (const optional_data& o): v_ (o.v_) {if (v_) new (&d_) T (o.d_);}

      optional_data (optional_data&& o)
        noexcept (std::is_nothrow_move_constructible<T>::value)
        : v_ (o.v_) {if (v_) new (&d_) T (std::move (o.d_));}
#endif

      optional_data& operator= (nullopt_t);
      optional_data& operator= (const T&);
      optional_data& operator= (T&&);

      optional_data& operator= (const optional_data&);

      // Note: it is trivially destructible and thus is no-throw destructible.
      //
      optional_data& operator= (optional_data&&)
        noexcept (std::is_nothrow_move_constructible<T>::value &&
                  std::is_nothrow_move_assignable<T>::value);
    };

    template <typename T,
              bool = std::is_copy_constructible<T>::value,
              bool = std::is_move_constructible<T>::value>
    struct optional_ctors: optional_data<T>
    {
      using optional_data<T>::optional_data;
    };

    template <typename T>
    struct optional_ctors<T, false, true>: optional_ctors<T, true, true>
    {
      using optional_ctors<T, true, true>::optional_ctors;

#if !defined(_MSC_VER) || _MSC_VER > 1900
      constexpr optional_ctors () = default;
#else
      optional_ctors () = default;
#endif

      optional_ctors (const optional_ctors&) = delete;

#if (!defined(_MSC_VER) || _MSC_VER > 1900) &&                  \
    (!defined(__GNUC__) || __GNUC__ > 4 || defined(__clang__))
      constexpr optional_ctors (optional_ctors&&) = default;
#else
      optional_ctors (optional_ctors&&) = default;
#endif

      optional_ctors& operator= (const optional_ctors&) = default;
      optional_ctors& operator= (optional_ctors&&) = default;
    };

    template <typename T>
    struct optional_ctors<T, true, false>: optional_ctors<T, true, true>
    {
      using optional_ctors<T, true, true>::optional_ctors;

#if !defined(_MSC_VER) || _MSC_VER > 1900
      constexpr optional_ctors () = default;
#else
      optional_ctors () = default;
#endif

#if (!defined(_MSC_VER) || _MSC_VER > 1900) &&                  \
    (!defined(__GNUC__) || __GNUC__ > 4 || defined(__clang__))
      constexpr optional_ctors (const optional_ctors&) = default;
#else
      optional_ctors (const optional_ctors&) = default;
#endif

      optional_ctors (optional_ctors&&) = delete;

      optional_ctors& operator= (const optional_ctors&) = default;
      optional_ctors& operator= (optional_ctors&&) = default;
    };

    template <typename T>
    struct optional_ctors<T, false, false>: optional_ctors<T, true, true>
    {
      using optional_ctors<T, true, true>::optional_ctors;

#if !defined(_MSC_VER) || _MSC_VER > 1900
      constexpr optional_ctors () = default;
#else
      optional_ctors () = default;
#endif

      optional_ctors (const optional_ctors&) = delete;
      optional_ctors (optional_ctors&&) = delete;

      optional_ctors& operator= (const optional_ctors&) = default;
      optional_ctors& operator= (optional_ctors&&) = default;
    };
  }

  template <typename T>
  class optional: private details::optional_ctors<T>
  {
    using base = details::optional_ctors<T>;

  public:
    using value_type = T;

#if !defined(_MSC_VER) || _MSC_VER > 1900
    constexpr optional ()                                 {}
    constexpr optional (nullopt_t)                        {}
    constexpr optional (const T& v): base (v)             {}
    constexpr optional (T&& v):      base (std::move (v)) {}
#else
    optional ()                                 {}
    optional (nullopt_t)                        {}
    optional (const T& v): base (v)             {}
    optional (T&& v):      base (std::move (v)) {}
#endif

#if (!defined(_MSC_VER) || _MSC_VER > 1900) &&  \
    (!defined(__GNUC__) || __GNUC__ > 4 || defined(__clang__))
    constexpr optional (const optional&) = default;
    constexpr optional (optional&&)      = default;
#else
    optional (const optional&) = default;
    optional (optional&&)      = default;
#endif

    optional& operator= (nullopt_t v) {static_cast<base&> (*this) = v;             return *this;}
    optional& operator= (const T& v)  {static_cast<base&> (*this) = v;             return *this;}
    optional& operator= (T&& v)       {static_cast<base&> (*this) = std::move (v); return *this;}

    optional& operator= (const optional&) = default;
    optional& operator= (optional&&)      = default;

    T&       value ()       {return this->d_;}
    const T& value () const {return this->d_;}

    T*       operator-> ()       {return &this->d_;}
    const T* operator-> () const {return &this->d_;}

    T&       operator* ()       {return this->d_;}
    const T& operator* () const {return this->d_;}

    bool         has_value () const {return this->v_;}
    explicit operator bool () const {return this->v_;}
  };

  // optional ? optional
  //
  template <typename T>
  inline auto
  operator== (const optional<T>& x, const optional<T>& y)
  {
    bool px (x), py (y);
    return px == py && (!px || *x == *y);
  }

  template <typename T>
  inline auto
  operator!= (const optional<T>& x, const optional<T>& y)
  {
    return !(x == y);
  }

  template <typename T>
  inline auto
  operator< (const optional<T>& x, const optional<T>& y)
  {
    bool px (x), py (y);
    return px < py || (px && py && *x < *y);
  }

  template <typename T>
  inline auto
  operator> (const optional<T>& x, const optional<T>& y)
  {
    return y < x;
  }

  // optional ? nullopt
  // nullopt ? optional
  //
  template <typename T>
  inline auto
  operator== (const optional<T>& x, nullopt_t)
  {
    bool px (x);
    return !px;
  }

  template <typename T>
  inline auto
  operator== (nullopt_t, const optional<T>& y)
  {
    bool py (y);
    return !py;
  }

  template <typename T>
  inline auto
  operator!= (const optional<T>& x, nullopt_t y)
  {
    return !(x == y);
  }

  template <typename T>
  inline auto
  operator!= (nullopt_t x, const optional<T>& y)
  {
    return !(x == y);
  }

  template <typename T>
  inline auto
  operator< (const optional<T>&, nullopt_t)
  {
    return false;
  }

  template <typename T>
  inline auto
  operator< (nullopt_t, const optional<T>& y)
  {
    bool py (y);
    return py;
  }

  template <typename T>
  inline auto
  operator> (const optional<T>& x, nullopt_t y)
  {
    return y < x;
  }

  template <typename T>
  inline auto
  operator> (nullopt_t x, const optional<T>& y)
  {
    return y < x;
  }

  // optional ? T
  // T ? optional
  //
  template <typename T>
  inline auto
  operator== (const optional<T>& x, const T& y)
  {
    bool px (x);
    return px && *x == y;
  }

  template <typename T>
  inline auto
  operator== (const T& x, const optional<T>& y)
  {
    bool py (y);
    return py && x == *y;
  }

  template <typename T>
  inline auto
  operator!= (const optional<T>& x, const T& y)
  {
    return !(x == y);
  }

  template <typename T>
  inline auto
  operator!= (const T& x, const optional<T>& y)
  {
    return !(x == y);
  }

  template <typename T>
  inline auto
  operator< (const optional<T>& x, const T& y)
  {
    bool px (x);
    return !px || *x < y;
  }

  template <typename T>
  inline auto
  operator< (const T& x, const optional<T>& y)
  {
    bool py (y);
    return py && x < *y;
  }

  template <typename T>
  inline auto
  operator> (const optional<T>& x, const T& y)
  {
    return y < x;
  }

  template <typename T>
  inline auto
  operator> (const T& x, const optional<T>& y)
  {
    return y < x;
  }
}

namespace std
{
  template <typename T>
  struct hash<butl::optional<T>>: hash<T>
  {
    using argument_type = butl::optional<T>;

    size_t
    operator() (const butl::optional<T>& o) const
      noexcept (noexcept (hash<T> {} (*o)))
    {
      return o ? hash<T>::operator() (*o) : static_cast<size_t> (-3333);
    }
  };
}

#include <libbutl/optional.ixx>

#endif // !LIBBUTL_STD_OPTIONAL
