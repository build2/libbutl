// file      : libbutl/curl.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules
#include <libbutl/curl.mxx>
#endif

// C includes.

#ifndef __cpp_lib_modules
#include <string>

#include <utility>   // move()
#include <exception> // invalid_argument
#endif

// Other includes.

#ifdef __cpp_modules
module butl.curl;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules
import std.core;
#endif
import butl.path;
import butl.process;
import butl.fdstream;
import butl.small_vector;
#endif

import butl.utility; // casecmp()
#else
#include <libbutl/utility.mxx>
#endif

using namespace std;

namespace butl
{
  int curl::
  map_in (nullfd_t, method_proto mp, io_data& d)
  {
    switch (mp)
    {
    case ftp_put:
      throw invalid_argument ("no input specified for PUT method");
    case http_post:
      throw invalid_argument ("no input specified for POST method");
    case ftp_get:
    case http_get:
      {
        d.pipe.in = fdnull (); // /dev/null
        return d.pipe.in.get ();
      }
    }

    return -1;
  }

  int curl::
  map_in (const path& f, method_proto mp, io_data& d)
  {
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
          out.open (move (d.pipe.out));
        }
        else
          d.pipe.in = fdnull (); // /dev/null

        return d.pipe.in.get ();
      }
    case ftp_get:
    case http_get:
      {
        throw invalid_argument ("file input specified for GET method");
      }
    }

    return -1;
  }

  int curl::
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
        d.pipe.out = fdnull ();
        return d.pipe.out.get (); // /dev/null
      }
    }

    return -1;
  }

  int curl::
  map_out (const path& f, method_proto mp, io_data& d)
  {
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
          in.open (move (d.pipe.in));
        }
        else
        {
          d.options.push_back ("-o");
          d.options.push_back (f.string ().c_str ());
          d.pipe.out = fdnull (); // /dev/null
        }

        return d.pipe.out.get ();
      }
    case ftp_put:
      {
        throw invalid_argument ("file output specified for PUT method");
      }
    }

    return -1;
  }

  curl::method_proto curl::
  translate (method_type m, const string& u, method_proto_options& o)
  {
    size_t n (u.find ("://"));

    if (n == string::npos)
      throw invalid_argument ("no protocol in URL");

    if (casecmp (u, "ftp", n) == 0 ||
        casecmp (u, "tftp", n) == 0)
    {
      switch (m)
      {
      case method_type::get: return method_proto::ftp_get;
      case method_type::put: return method_proto::ftp_put;
      case method_type::post:
        throw invalid_argument ("POST method with FTP protocol");
      }
    }
    else if (casecmp (u, "http", n) == 0 ||
             casecmp (u, "https", n) == 0)
    {
      o.push_back ("--fail");     // Fail on HTTP errors (e.g., 404).
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
}
