// file      : libbutl/openssl.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <cstddef> // size_t
#include <utility> // forward()

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

  template <typename E>
  inline optional<openssl_info> openssl::
  info (E&& err, const process_env& env)
  {
    return info ([] (const char* [], std::size_t) {},
                 std::forward<E> (err),
                 env);
  }
}
