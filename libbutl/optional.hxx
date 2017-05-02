// file      : libbutl/optional.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_OPTIONAL_HXX
#define LIBBUTL_OPTIONAL_HXX

#include <utility>    // move()
#include <functional> // hash

namespace butl
{
  // Simple optional class template while waiting for std::optional.
  //
  struct nullopt_t {constexpr explicit nullopt_t (int) {}};
  constexpr const nullopt_t nullopt (1);

  template <typename T>
  class optional
  {
  public:
    typedef T value_type;

    constexpr optional (): value_ (), null_ (true) {} // VC14 needs value_().
    constexpr optional (nullopt_t): value_ (), null_ (true) {}
    constexpr optional (const T& v): value_ (v), null_ (false) {}
    constexpr optional (T&& v): value_ (std::move (v)), null_ (false) {}

    optional& operator= (nullopt_t) {value_ = T (); null_ = true; return *this;}
    optional& operator= (const T& v) {value_ = v; null_ = false; return *this;}
    optional& operator= (T&& v) {value_ = std::move (v); null_ = false; return *this;}

    T&       value () {return value_;}
    const T& value () const {return value_;}

    T*       operator-> () {return &value_;}
    const T* operator-> () const {return &value_;}

    T&       operator* () {return value_;}
    const T& operator* () const {return value_;}

    bool         has_value () const {return !null_;}
    explicit operator bool () const {return !null_;}

  private:
    T value_;
    bool null_;
  };

  template <typename T>
  inline auto
  operator== (const optional<T>& x, const optional<T>& y) -> decltype (*x == *y)
  {
    return static_cast<bool> (x) == static_cast<bool> (y) && (!x || *x == *y);
  }

  template <typename T>
  inline auto
  operator!= (const optional<T>& x, const optional<T>& y) -> decltype (x == y)
  {
    return !(x == y);
  }

  template <typename T>
  inline auto
  operator< (const optional<T>& x, const optional<T>& y) -> decltype (*x < *y)
  {
    bool px (x), py (y);
    return px < py || (px && py && *x < *y);
  }

  template <typename T>
  inline auto
  operator> (const optional<T>& x, const optional<T>& y) -> decltype (*x > *y)
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

#endif // LIBBUTL_OPTIONAL_HXX