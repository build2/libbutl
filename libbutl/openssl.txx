// file      : libbutl/openssl.txx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <cstddef> // size_t
#include <utility> // forward()

namespace butl
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

  template <typename C,
            typename E>
  optional<openssl_info> openssl::
  info (const C& cmdc, E&& err, const process_env& env)
  {
    using namespace std;

    // Run the `openssl version` command.
    //
    openssl os (cmdc,
                nullfd, fdstream_mode::text, forward<E> (err),
                env,
                "version");

    // Read the command's stdout and wait for its completion. Bail out if the
    // command didn't terminate successfully or stdout contains no data.
    //
    string s;
    if (!getline (os.in, s))
      return nullopt;

    os.in.close ();

    if (!os.wait ())
      return nullopt;

    // Parse the version string.
    //
    // Note that there is some variety in the version representations:
    //
    // OpenSSL 3.0.0 7 sep 2021 (Library: OpenSSL 3.0.0 7 sep 2021)
    // OpenSSL 1.1.1l  FIPS 24 Aug 2021
    // LibreSSL 2.8.3
    //
    // We will only consider the first two space separated components as the
    // program name and version. We will also assume that there are no leading
    // spaces and the version is delimited from the program name with a single
    // space character.
    //
    size_t e (s.find (' '));

    // Bail out if there is no version present in the string or the program
    // name is empty.
    //
    if (e == string::npos || e == 0)
      return nullopt;

    string nm (s, 0, e);

    size_t b (e + 1);    // The beginning of the version.
    e = s.find (' ', b); // The end of the version.

    optional<semantic_version> ver (
      parse_semantic_version (string (s, b, e != string::npos ? e - b : e),
                              semantic_version::allow_build,
                              "" /* build_separators */));

    if (!ver)
      return nullopt;

    return openssl_info {move (nm), move (*ver)};
  }
}
