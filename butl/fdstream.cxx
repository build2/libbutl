// file      : butl/fdstream.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/fdstream>

#ifndef _WIN32
#  include <unistd.h>    // close(), read(), write()
#else
#  include <io.h>        // _close(), _read(), _write()
#endif

#include <system_error>

using namespace std;

namespace butl
{
  fdbuf::
  ~fdbuf () {close ();}

  void fdbuf::
  open (int fd)
  {
    close ();
    fd_ = fd;
    setg (buf_, buf_, buf_);
    setp (buf_, buf_ + sizeof (buf_) - 1); // Keep space for overflow's char.
  }

  void fdbuf::
  close ()
  {
    if (is_open ())
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
    int_type r (traits_type::eof ());

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

  fdbuf::int_type fdbuf::
  overflow (int_type c)
  {
    int_type r (traits_type::eof ());

    if (is_open () && c != traits_type::eof ())
    {
      // Store last character in the space we reserved in open(). Note
      // that pbump() doesn't do any checks.
      //
      *pptr () = traits_type::to_char_type (c);
      pbump (1);

      if (save ())
        r = c;
    }

    return r;
  }

  int fdbuf::
  sync ()
  {
    return is_open () && save () ? 0 : -1;
  }

  bool fdbuf::
  save ()
  {
    size_t n (pptr () - pbase ());

    if (n != 0)
    {
#ifndef _WIN32
      ssize_t m (::write (fd_, buf_, n));
#else
      int m (_write (fd_, buf_, static_cast<unsigned int> (sizeof (buf_))));
#endif

      if (m == -1)
        throw system_error (errno, system_category ());

      if (n != static_cast<size_t> (m))
        return false;

      setp (buf_, buf_ + sizeof (buf_) - 1);
    }

    return true;
  }
}
