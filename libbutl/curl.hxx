// file      : libbutl/curl.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <cstdint>     // uint16_t
#include <type_traits>

#include <libbutl/path.hxx>
#include <libbutl/process.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/small-vector.hxx>

#include <libbutl/export.hxx>

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
  //         fdopen_null (), // Write result to /dev/null.
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

    // By default the -sS and, for the HTTP protocol, --fail and --location
    // options are passed to curl on the command line. Optionally, these
    // options can be suppressed.
    //
    enum class flags: std::uint16_t
    {
      no_fail     = 0x01, // Don't pass --fail.
      no_location = 0x02, // Don't pass --location
      no_sS       = 0x04, // Don't pass -sS

      none = 0            // Default options set.
    };

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

    // Similar to the above, but allows to adjust the curl's default command
    // line.
    //
    template <typename I,
              typename O,
              typename E,
              typename... A>
    curl (I&& in,
          O&& out,
          E&& err,
          method_type,
          flags,
          const std::string& url,
          A&&... options);

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
          flags,
          const std::string& url,
          A&&... options);

    // Read the HTTP response status from an input stream.
    //
    // Specifically, read and parse the HTTP status line, by default skip over
    // the remaining headers (leaving the stream at the beginning of the
    // response body), and return the status code and the reason phrase. Throw
    // std::invalid_argument if the status line could not be parsed. Pass
    // through the ios::failure exception on the stream error.
    //
    // Note that if ios::failure is thrown the stream's exception mask may not
    // be preserved.
    //
    struct http_status
    {
      std::uint16_t code;
      std::string reason;
    };

    static http_status
    read_http_status (ifdstream&, bool skip_headers = true);

    // Parse and return the HTTP status code. Return 0 if the argument is
    // invalid.
    //
    static std::uint16_t
    parse_http_status_code (const std::string&);

    // Read the CRLF-terminated line from an input stream, stripping the
    // trailing CRLF. Pass through the ios::failure exception on the stream
    // error.
    //
    static std::string
    read_http_response_line (ifdstream&);

  private:
    enum method_proto {ftp_get, ftp_put, http_get, http_post};
    using method_proto_options = small_vector<const char*, 2>;

    method_proto
    translate (method_type,
               const std::string& url,
               method_proto_options&,
               flags);

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

    pipe
    map_in (nullfd_t, method_proto, io_data&);

    pipe
    map_in (const path&, method_proto, io_data&);

    template <typename I>
    typename std::enable_if<is_other<I>::value, I>::type
    map_in (I&&, method_proto, io_data&);

    pipe
    map_out (nullfd_t, method_proto, io_data&);

    pipe
    map_out (const path&, method_proto, io_data&);

    template <typename O>
    typename std::enable_if<is_other<O>::value, O>::type
    map_out (O&&, method_proto, io_data&);
  };

  curl::flags operator&  (curl::flags, curl::flags);
  curl::flags operator|  (curl::flags, curl::flags);
  curl::flags operator&= (curl::flags&, curl::flags);
  curl::flags operator|= (curl::flags&, curl::flags);
}

#include <libbutl/curl.ixx>
#include <libbutl/curl.txx>
