// file      : libbutl/curl.txx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  template <typename I>
  typename std::enable_if<curl::is_other<I>::value, I>::type curl::
  map_in (I&& in, method_proto mp, io_data& d)
  {
    switch (mp)
    {
    case ftp_put:
      {
        d.options.push_back ("--upload-file");
        d.options.push_back ("-");
        break;
      }
    case http_post:
      {
        d.options.push_back ("--data-binary");
        d.options.push_back ("@-");
        break;
      }
    case ftp_get:
    case http_get:
      {
        throw std::invalid_argument ("input specified for GET method");
      }
    }

    return std::forward<I> (in);
  }

  template <typename O>
  typename std::enable_if<curl::is_other<O>::value, O>::type curl::
  map_out (O&& out, method_proto mp, io_data&)
  {
    switch (mp)
    {
    case ftp_get:
    case http_get:
    case http_post:
      {
        // Note: no need for any options, curl writes to stdout by default.
        //
        break;
      }
    case ftp_put:
      {
        throw std::invalid_argument ("output specified for PUT method");
      }
    }

    return std::forward<O> (out);
  }

  template <typename C,
            typename I,
            typename O,
            typename E,
            typename... A>
  curl::
  curl (const C& cmdc,
        I&& in,
        O&& out,
        E&& err,
        method_type m,
        flags fs,
        const std::string& url,
        A&&... options)
  {
    method_proto_options mpo;
    method_proto mp (translate (m, url, mpo, fs));

    io_data in_data;
    io_data out_data;

    process& p (*this);
    p = process_start_callback (
      cmdc,
      map_in  (std::forward<I> (in),  mp, in_data),
      map_out (std::forward<O> (out), mp, out_data),
      std::forward<E> (err),
      "curl",
      ((fs & flags::no_sS) == flags::none
       ? "-sS" // Silent but do show diagnostics.
       : nullptr),
      mpo,
      in_data.options,
      out_data.options,
      std::forward<A> (options)...,
      url);

    // Note: leaving this scope closes any open ends of the pipes in io_data.
  }
}
