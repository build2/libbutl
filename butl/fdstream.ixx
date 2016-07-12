// file      : butl/fdstream.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

namespace butl
{
  // ifdstream
  //
  inline ifdstream::
  ifdstream ()
      : std::istream (&buf_)
  {
    exceptions (badbit | failbit);
  }

  inline ifdstream::
  ifdstream (int fd, iostate e)
      : fdstream_base (fd), std::istream (&buf_)
  {
    assert (e & badbit);
    exceptions (e);
  }

  inline ifdstream::
  ifdstream (int fd, fdstream_mode m, iostate e)
      : fdstream_base (fd, m),
        std::istream (&buf_),
        skip_ ((m & fdstream_mode::skip) == fdstream_mode::skip)
  {
    assert (e & badbit);
    exceptions (e);
  }

  inline ifdstream::
  ifdstream (const std::string& f, openmode m, iostate e)
      : ifdstream (f.c_str (), m, e) // Delegate.
  {
  }

  inline ifdstream::
  ifdstream (const path& f, openmode m, iostate e)
      : ifdstream (f.string (), m, e) // Delegate.
  {
  }

  inline ifdstream::
  ifdstream (const std::string& f, fdopen_mode m, iostate e)
      : ifdstream (f.c_str (), m, e) // Delegate.
  {
  }

  inline ifdstream::
  ifdstream (const path& f, fdopen_mode m, iostate e)
      : ifdstream (f.string (), m, e) // Delegate.
  {
  }

  inline void ifdstream::
  open (const std::string& f, openmode m)
  {
    open (f.c_str (), m);
  }

  inline void ifdstream::
  open (const path& f, openmode m)
  {
    open (f.string (), m);
  }

  inline void ifdstream::
  open (const std::string& f, fdopen_mode m)
  {
    open (f.c_str (), m);
  }

  inline void ifdstream::
  open (const path& f, fdopen_mode m)
  {
    open (f.string (), m);
  }

  // ofdstream
  //
  inline ofdstream::
  ofdstream ()
      : std::ostream (&buf_)
  {
    exceptions (badbit | failbit);
  }

  inline ofdstream::
  ofdstream (int fd, iostate e)
      : fdstream_base (fd), std::ostream (&buf_)
  {
    assert (e & badbit);
    exceptions (e);
  }

  inline ofdstream::
  ofdstream (int fd, fdstream_mode m, iostate e)
      : fdstream_base (fd, m), std::ostream (&buf_)
  {
    assert (e & badbit);
    exceptions (e);
  }

  inline ofdstream::
  ofdstream (const std::string& f, openmode m, iostate e)
      : ofdstream (f.c_str (), m, e) // Delegate.
  {
  }

  inline ofdstream::
  ofdstream (const path& f, openmode m, iostate e)
      : ofdstream (f.string (), m, e) // Delegate.
  {
  }

  inline ofdstream::
  ofdstream (const std::string& f, fdopen_mode m, iostate e)
      : ofdstream (f.c_str (), m, e) // Delegate.
  {
  }

  inline ofdstream::
  ofdstream (const path& f, fdopen_mode m, iostate e)
      : ofdstream (f.string (), m, e) // Delegate.
  {
  }

  inline void ofdstream::
  open (const std::string& f, openmode m)
  {
    open (f.c_str (), m);
  }

  inline void ofdstream::
  open (const path& f, openmode m)
  {
    open (f.string (), m);
  }

  inline void ofdstream::
  open (const std::string& f, fdopen_mode m)
  {
    open (f.c_str (), m);
  }

  inline void ofdstream::
  open (const path& f, fdopen_mode m)
  {
    open (f.string (), m);
  }

  // fdopen()
  //
  inline int
  fdopen (const std::string& f, fdopen_mode m, permissions p)
  {
    return fdopen (f.c_str (), m, p);
  }

  inline int
  fdopen (const path& f, fdopen_mode m, permissions p)
  {
    return fdopen (f.string (), m, p);
  }

  // fdstream_mode
  //
  inline fdstream_mode
  operator& (fdstream_mode x, fdstream_mode y)
  {
    return x &= y;
  }

  inline fdstream_mode
  operator| (fdstream_mode x, fdstream_mode y)
  {
    return x |= y;
  }

  inline fdstream_mode
  operator&= (fdstream_mode& x, fdstream_mode y)
  {
    return x = static_cast<fdstream_mode> (
      static_cast<std::uint16_t> (x) &
      static_cast<std::uint16_t> (y));
  }

  inline fdstream_mode
  operator|= (fdstream_mode& x, fdstream_mode y)
  {
    return x = static_cast<fdstream_mode> (
      static_cast<std::uint16_t> (x) |
      static_cast<std::uint16_t> (y));
  }

  // fdopen_mode
  //
  inline fdopen_mode operator& (fdopen_mode x, fdopen_mode y) {return x &= y;}
  inline fdopen_mode operator| (fdopen_mode x, fdopen_mode y) {return x |= y;}

  inline fdopen_mode
  operator&= (fdopen_mode& x, fdopen_mode y)
  {
    return x = static_cast<fdopen_mode> (
      static_cast<std::uint16_t> (x) &
      static_cast<std::uint16_t> (y));
  }

  inline fdopen_mode
  operator|= (fdopen_mode& x, fdopen_mode y)
  {
    return x = static_cast<fdopen_mode> (
      static_cast<std::uint16_t> (x) |
      static_cast<std::uint16_t> (y));
  }
}
