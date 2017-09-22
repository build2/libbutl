// file      : libbutl/openssl.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules
#include <libbutl/openssl.mxx>
#endif

#include <cassert>

#ifndef __cpp_lib_modules
#include <string>

#include <utility> // move()
#endif

// Other includes.

#ifdef __cpp_modules
module butl.openssl;

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

#endif

using namespace std;

namespace butl
{
  int openssl::
  map_in (nullfd_t, io_data& d)
  {
    d.pipe.in = fdnull (); // /dev/null
    return d.pipe.in.get ();
  }

  int openssl::
  map_in (const path& f, io_data& d)
  {
    if (f.string () == "-")
    {
      // Note: no need for any options, openssl reads from stdin by default.
      //
      d.pipe = fdopen_pipe (fdopen_mode::binary);
      out.open (move (d.pipe.out));
    }
    else
    {
      d.options.push_back ("-in");
      d.options.push_back (f.string ().c_str ());
      d.pipe.in = fdnull (); // /dev/null
    }

    return d.pipe.in.get ();
  }

  int openssl::
  map_in (fdstream_mode m, io_data& d)
  {
    assert (m == fdstream_mode::text || m == fdstream_mode::binary);

    // Note: no need for any options, openssl reads from stdin by default.
    //
    d.pipe = fdopen_pipe (m == fdstream_mode::binary
                          ? fdopen_mode::binary
                          : fdopen_mode::none);

    out.open (move (d.pipe.out));
    return d.pipe.in.get ();
  }

  int openssl::
  map_out (nullfd_t, io_data& d)
  {
    d.pipe.out = fdnull ();
    return d.pipe.out.get (); // /dev/null
  }

  int openssl::
  map_out (const path& f, io_data& d)
  {
    if (f.string () == "-")
    {
      // Note: no need for any options, openssl writes to stdout by default.
      //
      d.pipe = fdopen_pipe (fdopen_mode::binary);
      in.open (move (d.pipe.in), fdstream_mode::skip);
    }
    else
    {
      d.options.push_back ("-out");
      d.options.push_back (f.string ().c_str ());
      d.pipe.out = fdnull (); // /dev/null
    }

    return d.pipe.out.get ();
  }

  int openssl::
  map_out (fdstream_mode m, io_data& d)
  {
    assert (m == fdstream_mode::text || m == fdstream_mode::binary);

    // Note: no need for any options, openssl writes to stdout by default.
    //
    d.pipe = fdopen_pipe (m == fdstream_mode::binary
                          ? fdopen_mode::binary
                          : fdopen_mode::none);

    in.open (move (d.pipe.in), fdstream_mode::skip);
    return d.pipe.out.get ();
  }
}
