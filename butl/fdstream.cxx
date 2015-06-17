// file      : butl/fdstream.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2015 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/fdstream>

#ifndef _WIN32
#  include <unistd.h>    // close, read
#else
#  include <io.h>        // _close, _read
#endif

#include <system_error>

using namespace std;

namespace butl
{
  fdbuf::
  ~fdbuf () {close ();}

  void fdbuf::
  close ()
  {
    if (fd_ != -1)
    {
#ifndef _WIN32
      ::close (fd_);
#else
      _close (fd_);
#endif
      fd_ = -1;
    }
  }

  streamsize fdbuf::
  showmanyc ()
  {
    return is_open () ? static_cast<streamsize> (egptr () - gptr ()) : -1;
  }

  fdbuf::int_type fdbuf::
  underflow ()
  {
    int_type r = traits_type::eof ();

    if (is_open ())
    {
      if (gptr () < egptr () || load ())
        r = traits_type::to_int_type (*gptr ());
    }

    return r;
  }

  bool fdbuf::
  load ()
  {
#ifndef _WIN32
    ssize_t n (::read (fd_, buf_, sizeof (buf_)));
#else
    int n (_read (fd_, buf_, sizeof (buf_)));
#endif

    if (n == -1)
      throw system_error (errno, system_category ());

    setg (buf_, buf_, buf_ + n);
    return n != 0;
  }
}
