// file      : libbutl/curl.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

LIBBUTL_MODEXPORT namespace butl //@@ MOD Clang needs this for some reason.
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
        const std::string& url,
        A&&... options)
      : curl ([] (const char* [], std::size_t) {},
              std::forward<I> (in),
              std::forward<O> (out),
              std::forward<E> (err),
              m,
              url,
              std::forward<A> (options)...)
  {
  }
}
