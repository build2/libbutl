// file      : libbutl/openssl.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cstddef> // size_t
#include <utility> // move(), forward()

namespace butl
{
  template <typename I,
            typename O,
            typename E,
            typename... A>
  inline openssl::
  openssl (I&& in,
           O&& out,
           E&& err,
           const process_env& env,
           const std::string& command,
           A&&... options)
      : openssl ([] (const char* [], std::size_t) {},
                 std::forward<I> (in),
                 std::forward<O> (out),
                 std::forward<E> (err),
                 env,
                 command,
                 std::forward<A> (options)...)
  {
  }
}