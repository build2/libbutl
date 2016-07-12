// file      : butl/fdstream.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/fdstream>

#ifndef _WIN32
#  include <fcntl.h>     // open(), O_*
#  include <unistd.h>    // close(), read(), write(), lseek()
#  include <sys/stat.h>  // S_I*
#  include <sys/types.h> // off_t
#else
#  include <io.h>       // _close(), _read(), _write(), _setmode(), _sopen(),
                        // _lseek()
#  include <share.h>    // _SH_DENYNO
#  include <stdio.h>    // _fileno(), stdin, stdout, stderr
#  include <fcntl.h>    // _O_*
#  include <sys/stat.h> // S_I*
#endif

#include <errno.h> // errno, E*

#include <ios>          // ios_base::openmode, ios_base::failure
#include <limits>       // numeric_limits
#include <cassert>
#include <exception>    // uncaught_exception()
#include <stdexcept>    // invalid_argument
#include <type_traits>
#include <system_error>

using namespace std;

namespace butl
{
  // throw_ios_failure
  //
  template <bool = is_base_of<system_error, ios_base::failure>::value>
  struct throw_ios
  {
    static void impl (error_code e, const char* m) {
      throw ios_base::failure (m, e);}
  };

  template <>
  struct throw_ios<false>
  {
    static void impl (error_code, const char* m) {throw ios_base::failure (m);}
  };

  inline void
  throw_ios_failure (int ev)
  {
    error_code ec (ev, system_category ());
    throw_ios<>::impl (ec, ec.message ().c_str ());
  }

  inline void
  throw_ios_failure (int ev, const char* m)
  {
    throw_ios<>::impl (error_code (ev, system_category ()), m);
  }

  // fdbuf
  //
  fdbuf::
  ~fdbuf ()
  {
    if (is_open ())
      fdclose (fd_); // Don't check for an error as not much we can do here.
  }

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
        throw_ios_failure (errno);

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
      throw_ios_failure (errno);

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
      // Note that for MinGW GCC (5.2.0) _write() returns 0 for a file
      // descriptor opened for read-only access (while -1 with errno EBADF is
      // expected). This is in contrast with VC's _write() and POSIX's write().
      //
#ifndef _WIN32
      ssize_t m (write (fd_, buf_, n));
#else
      int m (_write (fd_, buf_, n));
#endif

      if (m == -1)
        throw_ios_failure (errno);

      if (n != static_cast<size_t> (m))
        return false;

      setp (buf_, buf_ + sizeof (buf_) - 1);
    }

    return true;
  }

  // fdstream_base
  //
  fdstream_base::
  fdstream_base (int fd, fdstream_mode m)
      : fdstream_base (fd) // Delegate.
  {
    // Note that here we rely on fdstream_base() (and fdbuf() which it calls)
    // to not read from the file.
    //
    if (fd != -1 &&
        ((m & fdstream_mode::text) == fdstream_mode::text ||
         (m & fdstream_mode::binary) == fdstream_mode::binary))
      fdmode (fd, m);
  }

  static fdopen_mode
  translate_mode (ios_base::openmode m)
  {
    enum
    {
      in    = ios_base::in,
      out   = ios_base::out,
      app   = ios_base::app,
      bin   = ios_base::binary,
      trunc = ios_base::trunc,
      ate   = ios_base::ate
    };

    const fdopen_mode fd_in     (fdopen_mode::in);
    const fdopen_mode fd_out    (fdopen_mode::out);
    const fdopen_mode fd_inout  (fdopen_mode::in | fdopen_mode::out);
    const fdopen_mode fd_app    (fdopen_mode::append);
    const fdopen_mode fd_trunc  (fdopen_mode::truncate);
    const fdopen_mode fd_create (fdopen_mode::create);
    const fdopen_mode fd_bin    (fdopen_mode::binary);
    const fdopen_mode fd_ate    (fdopen_mode::at_end);

    fdopen_mode r;
    switch (m & ~(ate | bin))
    {
    case in               : r = fd_in                           ; break;
    case out              :
    case out | trunc      : r = fd_out   | fd_trunc | fd_create ; break;
    case app              :
    case out | app        : r = fd_out   | fd_app   | fd_create ; break;
    case out | in         : r = fd_inout                        ; break;
    case out | in | trunc : r = fd_inout | fd_trunc | fd_create ; break;
    case out | in | app   :
    case in  | app        : r = fd_inout | fd_app   | fd_create ; break;

    default: throw invalid_argument ("invalid open mode");
    }

    if (m & ate)
      r |= fd_ate;

    if (m & bin)
      r |= fd_bin;

    return r;
  }

  // ifdstream
  //
  ifdstream::
  ifdstream (const char* f, openmode m, iostate e)
      : ifdstream (f, translate_mode (m | in), e) // Delegate.
  {
  }

  ifdstream::
  ifdstream (const char* f, fdopen_mode m, iostate e)
      : ifdstream (fdopen (f, m | fdopen_mode::in), e) // Delegate.
  {
  }

  ifdstream::
  ~ifdstream ()
  {
    if (skip_ && is_open () && good ())
    {
      // Clear the exception mask to prevent ignore() from throwing.
      //
      exceptions (goodbit);
      ignore (numeric_limits<streamsize>::max ());
    }

    // Underlying file descriptor is closed by fdbuf dtor with errors (if any)
    // being ignored.
    //
  }

  void ifdstream::
  open (const char* f, openmode m)
  {
    open (f, translate_mode (m | in));
  }

  void ifdstream::
  open (const char* f, fdopen_mode m)
  {
    open (fdopen (f, m | fdopen_mode::in));
  }

  void ifdstream::
  close ()
  {
    if (skip_ && is_open () && good ())
      ignore (numeric_limits<streamsize>::max ());

    buf_.close ();
  }

  ifdstream&
  getline (ifdstream& is, string& s, char delim)
  {
    ifdstream::iostate eb (is.exceptions ());
    assert (eb & ifdstream::badbit);

    // Amend the exception mask to prevent exceptions being thrown by the C++
    // IO runtime to avoid incompatibility issues due to ios_base::failure ABI
    // fiasco (#66145). We will not restore the mask when ios_base::failure is
    // thrown by fdbuf since there is no way to "silently" restore it if the
    // corresponding bits are in the error state without the exceptions() call
    // throwing ios_base::failure. Not restoring exception mask on throwing
    // because of badbit should probably be ok since the stream is no longer
    // usable.
    //
    if (eb != ifdstream::badbit)
      is.exceptions (ifdstream::badbit);

    std::getline (is, s, delim);

    // Throw if any of the newly set bits are present in the exception mask.
    //
    if ((is.rdstate () & eb) != ifdstream::goodbit)
      throw_ios_failure (EIO, "getline failure");

    if (eb != ifdstream::badbit)
      is.exceptions (eb); // Restore exception mask.

    return is;
  }

  // ofdstream
  //
  ofdstream::
  ofdstream (const char* f, openmode m, iostate e)
      : ofdstream (f, translate_mode (m | out), e) // Delegate.
  {
  }

  ofdstream::
  ofdstream (const char* f, fdopen_mode m, iostate e)
      : ofdstream (fdopen (f, m | fdopen_mode::out), e) // Delegate.
  {
  }

  ofdstream::
  ~ofdstream ()
  {
    // Enforce explicit close(). Note that we may have false negatives but not
    // false positives. Specifically, we will fail to enforce if someone is
    // using ofdstream in a dtor being called while unwinding the stack due to
    // an exception.
    //
    assert (!is_open () || !good () || uncaught_exception ());
  }

  void ofdstream::
  open (const char* f, openmode m)
  {
    open (f, translate_mode (m | out));
  }

  void ofdstream::
  open (const char* f, fdopen_mode m)
  {
    open (fdopen (f, m | fdopen_mode::out));
  }

  // Utility functions
  //
  int
  fdopen (const char* f, fdopen_mode m, permissions p)
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

    int fd (open (f, of, pf));

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

    if (pass_perm && file_exists (path (f)))
    {
      // If the _O_CREAT flag is set then we need to clear it so that we can
      // omit the permissions. But if the _O_EXCL flag is set as well we can't
      // do that as fdopen() wouldn't fail as expected.
      //
      if (of & _O_EXCL)
        throw_ios_failure (EEXIST);

      of &= ~_O_CREAT;
      pass_perm = false;
    }

    int fd (pass_perm
            ? _sopen (f, of, _SH_DENYNO, pf)
            : _sopen (f, of, _SH_DENYNO));

#endif

    if (fd == -1)
      throw_ios_failure (errno);

    if (mode (fdopen_mode::at_end))
    {
#ifndef _WIN32
      bool r (lseek(fd, 0, SEEK_END) != static_cast<off_t>(-1));
#else
      bool r (_lseek(fd, 0, SEEK_END) != -1);
#endif

      // Note that in the case of an error we don't delete the newly created
      // file as we have no indication if it is a new one.
      //
      if (!r)
      {
        int e (errno);
        fdclose (fd); // Doesn't throw, but can change errno.
        throw_ios_failure (e);
      }
    }

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

  fdstream_mode
  fdmode (int, fdstream_mode)
  {
    return fdstream_mode::binary;
  }

  fdstream_mode
  stdin_fdmode (fdstream_mode)
  {
    return fdstream_mode::binary;
  }

  fdstream_mode
  stdout_fdmode (fdstream_mode)
  {
    return fdstream_mode::binary;
  }

  fdstream_mode
  stderr_fdmode (fdstream_mode)
  {
    return fdstream_mode::binary;
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

  fdstream_mode
  fdmode (int fd, fdstream_mode m)
  {
    m &= fdstream_mode::text | fdstream_mode::binary;

    // Should be exactly one translation flag specified.
    //
    if (m != fdstream_mode::binary && m != fdstream_mode::text)
      throw invalid_argument ("invalid translation mode");

    int r (_setmode (fd, m == fdstream_mode::binary ? _O_BINARY : _O_TEXT));
    if (r == -1)
      throw_ios_failure (errno);

    return (r & _O_BINARY) == _O_BINARY
      ? fdstream_mode::binary
      : fdstream_mode::text;
  }

  fdstream_mode
  stdin_fdmode (fdstream_mode m)
  {
    int fd (_fileno (stdin));
    if (fd == -1)
      throw_ios_failure (errno);

    return fdmode (fd, m);
  }

  fdstream_mode
  stdout_fdmode (fdstream_mode m)
  {
    int fd (_fileno (stdout));
    if (fd == -1)
      throw_ios_failure (errno);

    return fdmode (fd, m);
  }

  fdstream_mode
  stderr_fdmode (fdstream_mode m)
  {
    int fd (_fileno (stderr));
    if (fd == -1)
      throw_ios_failure (errno);

    return fdmode (fd, m);
  }

#endif
}
