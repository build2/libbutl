// file      : libbutl/curl.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_CURL_HXX
#define LIBBUTL_CURL_HXX

#include <string>
#include <type_traits>

#include <libbutl/export.hxx>

#include <libbutl/process.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/small-vector.hxx>

namespace butl
{
  // Perform a method (GET, POST, PUT) on a URL using the curl(1) program.
  // Throw process_error and io_error (both derive from system_error) in case
  // of errors.
  //
  // The I (in) and O (out) can be of the following types/values:
  //
  // nullfd   Signal that no input/output is expected.
  //
  // path     Read input/write output from/to a file. If the special "-"
  //          value is used, then instead input is connected to the curl::out
  //          ofdstream member and output -- to the curl::in ifdstream member.
  //          Note that the argument type should be path, not string (i.e.,
  //          pass path("-")).
  //
  // other    Forwarded as is to process_start(). Normally either int or
  //          auto_fd.
  //
  // For example:
  //
  //   curl (nullfd,     // No input expected for GET.
  //         path ("-"), // Write response to curl::in.
  //         2,
  //         curl::get,
  //         "http://example.org");
  //
  //   curl (path ("-"),         // Read request from curl::out.
  //         path::temp_path (), // Write result to a file.
  //         2,
  //         curl::post,
  //         "http://example.org");
  //
  //   curl (nullfd,
  //         fdnull (), // Write result to /dev/null.
  //         2,
  //         curl::get,
  //         "tftp://localhost/foo");
  //
  // Typical usage:
  //
  // try
  // {
  //   curl c (nullfd,                 // No input expected.
  //           path ("-"),             // Output to curl::in.
  //           2,                      // Diagnostics to stderr.
  //           curl::get,              // GET method.
  //           "https://example.org",
  //           "-A", "foobot/1.2.3");  // Additional curl(1) options.
  //
  //   for (string s; getline (c.in, s); )
  //     cout << s << endl;
  //
  //   c.in.close ();
  //
  //   if (!c.wait ())
  //     ... // curl returned non-zero status.
  // }
  // catch (const std::system_error& e)
  // {
  //   cerr << "curl error: " << e << endl;
  // }
  //
  // Notes:
  //
  // 1. If opened, in/out streams are in the binary mode.
  //
  // 2. If opened, in/out must be explicitly closed before calling wait().
  //
  // 3. Only binary data HTTP POST is currently supported (the --data-binary
  //    curl option).
  //
  class LIBBUTL_SYMEXPORT curl: public process
  {
  public:
    enum method_type {get, put, post};

    ifdstream in;
    ofdstream out;

    template <typename I,
              typename O,
              typename E,
              typename... A>
    curl (I&& in,
          O&& out,
          E&& err,
          method_type,
          const std::string& url,
          A&&... options);

    // Version with the command line callback (see process_run_callback() for
    // details).
    //
    template <typename C,
              typename I,
              typename O,
              typename E,
              typename... A>
    curl (const C&,
          I&& in,
          O&& out,
          E&& err,
          method_type,
          const std::string& url,
          A&&... options);

  private:
    enum method_proto {ftp_get, ftp_put, http_get, http_post};
    using method_proto_options = small_vector<const char*, 2>;

    method_proto
    translate (method_type, const std::string& url, method_proto_options&);

  private:
    template <typename T>
    struct is_other
    {
      using type = typename std::remove_reference<
        typename std::remove_cv<T>::type>::type;

      static const bool value = !(std::is_same<type, nullfd_t>::value ||
                                  std::is_same<type, path>::value);
    };

    struct io_data
    {
      fdpipe pipe;
      method_proto_options options;
      std::string storage;
    };

    int
    map_in (nullfd_t, method_proto, io_data&);

    int
    map_in (const path&, method_proto, io_data&);

    template <typename I>
    typename std::enable_if<is_other<I>::value, I>::type
    map_in (I&&, method_proto, io_data&);

    int
    map_out (nullfd_t, method_proto, io_data&);

    int
    map_out (const path&, method_proto, io_data&);

    template <typename O>
    typename std::enable_if<is_other<O>::value, O>::type
    map_out (O&&, method_proto, io_data&);
  };
}

#include <libbutl/curl.ixx>
#include <libbutl/curl.txx>

#endif // LIBBUTL_CURL_HXX
