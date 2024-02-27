// file      : libbutl/curl.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/curl.hxx>

#include <cassert>
#include <utility>   // move()
#include <cstdlib>   // strtoul(), size_t
#include <exception> // invalid_argument

#include <libbutl/utility.hxx>

using namespace std;

namespace butl
{
  process::pipe curl::
  map_in (nullfd_t, method_proto mp, io_data& d)
  {
    switch (mp)
    {
    case ftp_put:
      throw invalid_argument ("no input specified for PUT method");
    case http_post:
      {
        // Post the empty data.
        //
        // Note that while it's tempting to specify the --request POST option
        // instead, that can potentially overwrite the request methods for the
        // HTTP 30X response code redirects.
        //
        d.options.push_back ("--data-raw");
        d.options.push_back ("");
      }
      // Fall through.
    case ftp_get:
    case http_get:
      {
        d.pipe.in = fdopen_null (); // /dev/null
        return pipe (d.pipe);
      }
    }

    assert (false); // Can't be here.
    return pipe ();
  }

  process::pipe curl::
  map_in (const path& f, method_proto mp, io_data& d)
  {
    pipe r;
    switch (mp)
    {
    case ftp_put:
    case http_post:
      {
        if (mp == ftp_put)
        {
          d.options.push_back ("--upload-file");
          d.options.push_back (f.string ().c_str ());
        }
        else
        {
          d.storage = '@' + f.string ();

          d.options.push_back ("--data-binary");
          d.options.push_back (d.storage.c_str ());
        }

        if (f.string () == "-")
        {
          d.pipe = fdopen_pipe (fdopen_mode::binary);
          r = pipe (d.pipe);

          out.open (move (d.pipe.out));
        }
        else
        {
          d.pipe.in = fdopen_null (); // /dev/null
          r = pipe (d.pipe);
        }

        return r;
      }
    case ftp_get:
    case http_get:
      {
        throw invalid_argument ("file input specified for GET method");
      }
    }

    assert (false); // Can't be here.
    return r;
  }

  process::pipe curl::
  map_out (nullfd_t, method_proto mp, io_data& d)
  {
    switch (mp)
    {
    case ftp_get:
    case http_get:
      throw invalid_argument ("no output specified for GET method");
    case ftp_put:
    case http_post: // May or may not produce output.
      {
        d.pipe.out = fdopen_null ();
        return pipe (d.pipe); // /dev/null
      }
    }

    assert (false); // Can't be here.
    return pipe ();
  }

  process::pipe curl::
  map_out (const path& f, method_proto mp, io_data& d)
  {
    pipe r;
    switch (mp)
    {
    case ftp_get:
    case http_get:
    case http_post:
      {
        if (f.string () == "-")
        {
          // Note: no need for any options, curl writes to stdout by default.
          //
          d.pipe = fdopen_pipe (fdopen_mode::binary);
          r = pipe (d.pipe);

          in.open (move (d.pipe.in));
        }
        else
        {
          d.options.push_back ("-o");
          d.options.push_back (f.string ().c_str ());
          d.pipe.out = fdopen_null (); // /dev/null
          r = pipe (d.pipe);
        }

        return r;
      }
    case ftp_put:
      {
        throw invalid_argument ("file output specified for PUT method");
      }
    }

    assert (false); // Can't be here.
    return r;
  }

  curl::method_proto curl::
  translate (method_type m, const string& u, method_proto_options& o, flags fs)
  {
    size_t n (u.find ("://"));

    if (n == string::npos)
      throw invalid_argument ("no protocol in URL");

    if (icasecmp (u, "ftp", n) == 0 || icasecmp (u, "tftp", n) == 0)
    {
      switch (m)
      {
      case method_type::get: return method_proto::ftp_get;
      case method_type::put: return method_proto::ftp_put;
      case method_type::post:
        throw invalid_argument ("POST method with FTP protocol");
      }
    }
    else if (icasecmp (u, "http", n) == 0 || icasecmp (u, "https", n) == 0)
    {
      if ((fs & flags::no_fail) == flags::none)
        o.push_back ("--fail");     // Fail on HTTP errors (e.g., 404).

      if ((fs & flags::no_location) == flags::none)
        o.push_back ("--location"); // Follow redirects.

      switch (m)
      {
      case method_type::get:  return method_proto::http_get;
      case method_type::post: return method_proto::http_post;
      case method_type::put:
        throw invalid_argument ("PUT method with HTTP protocol");
      }
    }

    throw invalid_argument ("unsupported protocol");
  }

  uint16_t curl::
  parse_http_status_code (const string& s)
  {
    char* e (nullptr);
    unsigned long c (strtoul (s.c_str (), &e, 10)); // Can't throw.
    assert (e != nullptr);

    return *e == '\0' && c >= 100 && c < 600
           ? static_cast<uint16_t> (c)
           : 0;
  }

  string curl::
  read_http_response_line (ifdstream& is)
  {
    string r;
    getline (is, r); // Strips the trailing LF (0xA).

    // Note that on POSIX CRLF is not automatically translated into LF, so we
    // need to strip CR (0xD) manually.
    //
    if (!r.empty () && r.back () == '\r')
      r.pop_back ();

    return r;
  }

  curl::http_status curl::
  read_http_status (ifdstream& is, bool skip_headers)
  {
    // After getting the status line, if requested, we will read until the
    // empty line (containing just CRLF). Not being able to reach such a line
    // is an error, which is the reason for the exception mask choice. When
    // done, we will restore the original exception mask.
    //
    ifdstream::iostate es (is.exceptions ());
    is.exceptions (ifdstream::badbit | ifdstream::failbit | ifdstream::eofbit);

    auto read_status = [&is, es] ()
    {
      string l (read_http_response_line (is));

      for (;;) // Breakout loop.
      {
        if (l.compare (0, 5, "HTTP/") != 0)
          break;

        size_t p (l.find (' ', 5));             // The protocol end.
        if (p == string::npos)
          break;

        p = l.find_first_not_of (' ', p + 1);   // The code start.
        if (p == string::npos)
          break;

        size_t e (l.find (' ', p + 1));         // The code end.
        if (e == string::npos)
          break;

        uint16_t c (parse_http_status_code (string (l, p, e - p)));
        if (c == 0)
          break;

        string r;
        p = l.find_first_not_of (' ', e + 1);   // The reason start.
        if (p != string::npos)
        {
          e = l.find_last_not_of (' ');         // The reason end.
          assert (e != string::npos && e >= p);

          r = string (l, p, e - p + 1);
        }

        return http_status {c, move (r)};
      }

      is.exceptions (es); // Restore the exception mask.

      throw invalid_argument ("invalid status line '" + l + "'");
    };

    // The curl output for a successfull request looks like this:
    //
    // HTTP/1.1 100 Continue
    //
    // HTTP/1.1 200 OK
    // Content-Length: 83
    // Content-Type: text/manifest;charset=utf-8
    //
    // <response-body>
    //
    // curl normally sends the 'Expect: 100-continue' header for uploads, so
    // we need to handle the interim HTTP server response with the continue
    // (100) status code.
    //
    // Interestingly, Apache can respond with the continue (100) code and with
    // the not found (404) code afterwords.
    //
    http_status rs (read_status ());

    if (rs.code == 100)
    {
      // Skips the interim response.
      //
      while (!read_http_response_line (is).empty ()) ;

      rs = read_status (); // Reads the final status code.
    }

    if (skip_headers)
    {
      while (!read_http_response_line (is).empty ()) ; // Skips headers.
    }

    is.exceptions (es);

    return rs;
  }
}
