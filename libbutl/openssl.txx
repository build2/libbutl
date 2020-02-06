// file      : libbutl/openssl.txx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

LIBBUTL_MODEXPORT namespace butl //@@ MOD Clang needs this for some reason.
{
  template <typename I>
  typename std::enable_if<openssl::is_other<I>::value, I>::type openssl::
  map_in (I&& in, io_data&)
  {
    return std::forward<I> (in);
  }

  template <typename O>
  typename std::enable_if<openssl::is_other<O>::value, O>::type openssl::
  map_out (O&& out, io_data&)
  {
    return std::forward<O> (out);
  }

  template <typename C,
            typename I,
            typename O,
            typename E,
            typename... A>
  openssl::
  openssl (const C& cmdc,
           I&& in,
           O&& out,
           E&& err,
           const process_env& env,
           const std::string& command,
           A&&... options)
  {
    io_data in_data;
    io_data out_data;

    process& p (*this);
    p = process_start_callback (cmdc,
                                map_in  (std::forward<I> (in),  in_data),
                                map_out (std::forward<O> (out), out_data),
                                std::forward<E> (err),
                                env,
                                command,
                                in_data.options,
                                out_data.options,
                                std::forward<A> (options)...);

    // Note: leaving this scope closes any open ends of the pipes in io_data.
  }
}
