// file      : libbutl/curl.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <cstddef>   // size_t
#include <utility>   // forward()
#include <exception> // invalid_argument

namespace butl
{
  template <typename I,
            typename O,
            typename E,
            typename... A>
  inline curl::
  curl (I&& in,
        O&& out,
        E&& err,
        method_type m,
        flags fs,
        const std::string& url,
        A&&... options)
      : curl ([] (const char* [], std::size_t) {},
              std::forward<I> (in),
              std::forward<O> (out),
              std::forward<E> (err),
              m,
              fs,
              url,
              std::forward<A> (options)...)
  {
  }

  template <typename C,
            typename I,
            typename O,
            typename E,
            typename... A>
  inline curl::
  curl (const C& cmdc,
        I&& in,
        O&& out,
        E&& err,
        method_type m,
        const std::string& url,
        A&&... options)
      : curl (cmdc,
              std::forward<I> (in),
              std::forward<O> (out),
              std::forward<E> (err),
              m,
              flags::none,
              url,
              std::forward<A> (options)...)
  {
  }

  template <typename I,
            typename O,
            typename E,
            typename... A>
  inline curl::
  curl (I&& in,
        O&& out,
        E&& err,
        method_type m,
        const std::string& url,
        A&&... options)
      : curl (std::forward<I> (in),
              std::forward<O> (out),
              std::forward<E> (err),
              m,
              flags::none,
              url,
              std::forward<A> (options)...)
  {
  }

  inline curl::flags
  operator&= (curl::flags& x, curl::flags y)
  {
    return x = static_cast<curl::flags> (static_cast<std::uint16_t> (x) &
                                         static_cast<std::uint16_t> (y));
  }

  inline curl::flags
  operator|= (curl::flags& x, curl::flags y)
  {
    return x = static_cast<curl::flags> (static_cast<std::uint16_t> (x) |
                                         static_cast<std::uint16_t> (y));
  }

  inline curl::flags
  operator& (curl::flags x, curl::flags y)
  {
    return x &= y;
  }

  inline curl::flags
  operator| (curl::flags x, curl::flags y)
  {
    return x |= y;
  }
}
