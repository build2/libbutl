// file      : libbutl/openssl.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

LIBBUTL_MODEXPORT namespace butl //@@ MOD Clang needs this for some reason.
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
