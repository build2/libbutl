// file      : butl/fdstream.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/fdstream>

#ifndef _WIN32
#  include <fcntl.h>    // open(), O_*
#  include <unistd.h>   // close(), read(), write()
#  include <sys/stat.h> // S_I*
#else
#  include <io.h>       // _close(), _read(), _write(), _setmode(), _sopen()
#  include <share.h>    // _SH_DENYNO
#  include <stdio.h>    // _fileno(), stdin, stdout, stderr
#  include <fcntl.h>    // _O_*
#  include <sys/stat.h> // S_I*
#endif

#include <system_error>

using namespace std;

namespace butl
{
  // fdbuf
  //
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
      if (!fdclose (fd_))
        throw system_error (errno, system_category ());

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
    ssize_t n (read (fd_, buf_, sizeof (buf_)));
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
      ssize_t m (write (fd_, buf_, n));
#else
      int m (_write (fd_, buf_, n));
#endif

      if (m == -1)
        throw system_error (errno, system_category ());

      if (n != static_cast<size_t> (m))
        return false;

      setp (buf_, buf_ + sizeof (buf_) - 1);
    }

    return true;
  }

  // fdstream_base
  //
  fdstream_base::
  fdstream_base (int fd, fdtranslate m)
      : fdstream_base (fd) // Delegate.
  {
    // Note that here we rely on fdstream_base() (and fdbuf() which it calls)
    // to note read from the file.
    //
    fdmode (fd, m);
  }

  // Utility functions
  //
  int
  fdopen (const path& f, fdopen_mode m, permissions p)
  {
    mode_t pf (S_IREAD | S_IWRITE | S_IEXEC);

#ifdef S_IRWXG
    pf |= S_IRWXG;
#endif

#ifdef S_IRWXO
    pf |= S_IRWXO;
#endif

    pf &= static_cast<mode_t> (p);

    // Return true if the open mode contains a specific flag.
    //
    auto mode = [m](fdopen_mode flag) -> bool {return (m & flag) == flag;};

    int of (0);
    bool in (mode (fdopen_mode::in));
    bool out (mode (fdopen_mode::out));

#ifndef _WIN32

    if (in && out)
      of |= O_RDWR;
    else if (in)
      of |= O_RDONLY;
    else if (out)
      of |= O_WRONLY;

    if (out)
    {
      if (mode (fdopen_mode::append))
        of |= O_APPEND;

      if (mode (fdopen_mode::truncate))
        of |= O_TRUNC;
    }

    if (mode (fdopen_mode::create))
    {
      of |= O_CREAT;

      if (mode (fdopen_mode::exclusive))
        of |= O_EXCL;
    }

#ifdef O_LARGEFILE
    of |= O_LARGEFILE;
#endif

    int fd (open (f.string ().c_str (), of, pf));

#else

    if (in && out)
      of |= _O_RDWR;
    else if (in)
      of |= _O_RDONLY;
    else if (out)
      of |= _O_WRONLY;

    if (out)
    {
      if (mode (fdopen_mode::append))
        of |= _O_APPEND;

      if (mode (fdopen_mode::truncate))
        of |= _O_TRUNC;
    }

    if (mode (fdopen_mode::create))
    {
      of |= _O_CREAT;

      if (mode (fdopen_mode::exclusive))
        of |= _O_EXCL;
    }

    of |= mode (fdopen_mode::binary) ? _O_BINARY : _O_TEXT;

    // According to Microsoft _sopen() should not change the permissions of an
    // existing file. Meanwhile it does if we pass them (reproduced on Windows
    // XP, 7, and 8). And we must pass them if we have _O_CREATE. So we need
    // to take care of preserving the permissions ourselves. Note that Wine's
    // implementation of _sopen() works properly.
    //
    bool pass_perm (of & _O_CREAT);

    if (pass_perm && file_exists (f))
    {
      // If the _O_CREAT flag is set then we need to clear it so that we can
      // omit the permissions. But if the _O_EXCL flag is set as well we can't
      // do that as fdopen() wouldn't fail as expected.
      //
      if (of & _O_EXCL)
        throw system_error (EEXIST, system_category ());

      of &= ~_O_CREAT;
      pass_perm = false;
    }

    int fd (pass_perm
            ? _sopen (f.string ().c_str (), of, _SH_DENYNO, pf)
            : _sopen (f.string ().c_str (), of, _SH_DENYNO));

#endif

    if (fd == -1)
      throw system_error (errno, system_category ());

    return fd;
  }

#ifndef _WIN32

  bool
  fdclose (int fd) noexcept
  {
    return close (fd) == 0;
  }

  int
  fdnull () noexcept
  {
    return open ("/dev/null", O_RDWR);
  }

  fdtranslate
  fdmode (int, fdtranslate)
  {
    return fdtranslate::binary;
  }

  fdtranslate
  stdin_fdmode (fdtranslate)
  {
    return fdtranslate::binary;
  }

  fdtranslate
  stdout_fdmode (fdtranslate)
  {
    return fdtranslate::binary;
  }

  fdtranslate
  stderr_fdmode (fdtranslate)
  {
    return fdtranslate::binary;
  }

#else

  bool
  fdclose (int fd) noexcept
  {
    return _close (fd) == 0;
  }

  int
  fdnull () noexcept
  {
    return _sopen ("nul", _O_RDWR, _SH_DENYNO);
  }

  fdtranslate
  fdmode (int fd, fdtranslate m)
  {
    int r (_setmode (fd, m == fdtranslate::binary ? _O_BINARY : _O_TEXT));
    if (r == -1)
      throw system_error (errno, system_category ());

    return (r & _O_BINARY) == _O_BINARY
      ? fdtranslate::binary
      : fdtranslate::text;
  }

  fdtranslate
  stdin_fdmode (fdtranslate m)
  {
    int fd (_fileno (stdin));
    if (fd == -1)
      throw system_error (errno, system_category ());

    return fdmode (fd, m);
  }

  fdtranslate
  stdout_fdmode (fdtranslate m)
  {
    int fd (_fileno (stdout));
    if (fd == -1)
      throw system_error (errno, system_category ());

    return fdmode (fd, m);
  }

  fdtranslate
  stderr_fdmode (fdtranslate m)
  {
    int fd (_fileno (stderr));
    if (fd == -1)
      throw system_error (errno, system_category ());

    return fdmode (fd, m);
  }

#endif
}
