// file      : libbutl/fdstream.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules
#include <libbutl/fdstream.mxx>
#endif

#include <errno.h> // errno, E*

#ifndef _WIN32
#  include <fcntl.h>     // open(), O_*, fcntl()
#  include <unistd.h>    // close(), read(), write(), lseek(), dup(), pipe(),
                         // isatty(), ssize_t, STD*_FILENO
#  include <sys/uio.h>   // writev(), iovec
#  include <sys/stat.h>  // stat(), S_I*
#  include <sys/types.h> // stat, off_t
#else
#  include <libbutl/win32-utility.hxx>

#  include <io.h>       // _close(), _read(), _write(), _setmode(), _sopen(),
                        // _lseek(), _dup(), _pipe(), _get_osfhandle()
#  include <share.h>    // _SH_DENYNO
#  include <stdio.h>    // _fileno(), stdin, stdout, stderr
#  include <fcntl.h>    // _O_*
#  include <sys/stat.h> // S_I*

#  include <wchar.h> // wcsncmp(), wcsstr()
#endif

#include <cassert>

#ifndef __cpp_lib_modules
#include <vector>
#include <string>
#include <istream>
#include <ostream>
#include <utility>
#include <cstdint>

#include <ios>          // ios_base::openmode, ios_base::failure
#include <new>          // bad_alloc
#include <limits>       // numeric_limits
#include <cstring>      // memcpy(), memmove()
#include <exception>    // uncaught_exception[s]()
#include <stdexcept>    // invalid_argument
#include <system_error>
#endif

#include <libbutl/ft/exception.hxx>     // uncaught_exceptions
#include <libbutl/process-details.hxx>

#ifdef __cpp_modules
module butl.fdstream;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules
import std.core;
import std.io;
import std.threading; // Clang wants it in purview (see process-details.hxx).
#endif
import butl.path;
import butl.filesystem;
#endif

#endif

using namespace std;

#ifdef _WIN32
using namespace butl::win32;
#endif

namespace butl
{
  // auto_fd
  //
  void auto_fd::
  close ()
  {
    if (fd_ >= 0)
    {
      bool r (fdclose (fd_));

      // If fdclose() failed then no reason to expect it to succeed the next
      // time.
      //
      fd_ = -1;

      if (!r)
        throw_generic_ios_failure (errno);
    }
  }

  // fdbuf
  //
  fdbuf::
  fdbuf (auto_fd&& fd)
  {
    if (fd.get () >= 0)
      open (move (fd));
  }

  void fdbuf::
  open (auto_fd&& fd)
  {
    close ();

#ifndef _WIN32
    int flags (fcntl (fd.get (), F_GETFL));

    if (flags == -1)
      throw_generic_ios_failure (errno);

    non_blocking_ = (flags & O_NONBLOCK) == O_NONBLOCK;
#endif

    setg (buf_, buf_, buf_);
    setp (buf_, buf_ + sizeof (buf_) - 1); // Keep space for overflow's char.
    off_ = 0; // @@ Strictly speaking, need to query, can be at end.
    fd_ = move (fd);
  }

  streamsize fdbuf::
  showmanyc ()
  {
    if (!is_open ())
      return -1;

    streamsize n (egptr () - gptr ());

    if (n > 0)
      return n;

#ifndef _WIN32
    if (non_blocking_)
    {
      ssize_t n (read (fd_.get (), buf_, sizeof (buf_)));

      if (n == -1)
      {
        if (errno == EAGAIN || errno == EINTR)
          return 0;

        throw_generic_ios_failure (errno);
      }

      if (n == 0) // EOF.
        return -1;

      setg (buf_, buf_, buf_ + n);
      off_ += n;

      return n;
    }
#endif

    return 0;
  }

  fdbuf::int_type fdbuf::
  underflow ()
  {
    int_type r (traits_type::eof ());

    if (is_open ())
    {
      // The underflow() function interface doesn't support the non-blocking
      // semantics as it must return either the next character or EOF. In the
      // future we may implement the blocking behavior for a non-blocking file
      // descriptor.
      //
      if (non_blocking_)
        throw_generic_ios_failure (ENOTSUP);

      if (gptr () < egptr () || load ())
        r = traits_type::to_int_type (*gptr ());
    }

    return r;
  }

  bool fdbuf::
  load ()
  {
    // Doesn't handle blocking mode and so should not be called.
    //
    assert (!non_blocking_);

#ifndef _WIN32
    ssize_t n (read (fd_.get (), buf_, sizeof (buf_)));
#else
    int n (_read (fd_.get (), buf_, sizeof (buf_)));
#endif

    if (n == -1)
      throw_generic_ios_failure (errno);

    setg (buf_, buf_, buf_ + n);
    off_ += n;
    return n != 0;
  }

  fdbuf::int_type fdbuf::
  overflow (int_type c)
  {
    int_type r (traits_type::eof ());

    if (is_open () && c != traits_type::eof ())
    {
      // The overflow() function interface doesn't support the non-blocking
      // semantics since being unable to serialize the character is supposed
      // to be an error. In the future we may implement the blocking behavior
      // for a non-blocking file descriptor.
      //
      if (non_blocking_)
        throw_generic_ios_failure (ENOTSUP);

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
    if (!is_open ())
      return -1;

    // The sync() function interface doesn't support the non-blocking
    // semantics since it should either completely sync the data or fail. In
    // the future we may implement the blocking behavior for a non-blocking
    // file descriptor.
    //
    if (non_blocking_)
      throw_generic_ios_failure (ENOTSUP);

    return save () ? 0 : -1;
  }

#ifdef _WIN32
  static inline int
  write (int fd, const void* buf, size_t n)
  {
    return _write (fd, buf, static_cast<unsigned int> (n));
  }
#endif

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
      auto m (write (fd_.get (), buf_, n));

      if (m == -1)
        throw_generic_ios_failure (errno);

      off_ += m;

      if (n != static_cast<size_t> (m))
        return false;

      setp (buf_, buf_ + sizeof (buf_) - 1);
    }

    return true;
  }

  streamsize fdbuf::
  xsputn (const char_type* s, streamsize sn)
  {
    // The xsputn() function interface doesn't support the non-blocking
    // semantics since the only excuse not to fully serialize the data is
    // encountering EOF (the default behaviour is defined as a sequence of
    // sputc() calls which stops when either sn characters are written or a
    // call would have returned EOF). In the future we may implement the
    // blocking behavior for a non-blocking file descriptor.
    //
    if (non_blocking_)
      throw_generic_ios_failure (ENOTSUP);

    // To avoid futher 'signed/unsigned comparison' compiler warnings.
    //
    size_t n (static_cast<size_t> (sn));

    auto advance = [this] (size_t n) {pbump (static_cast<int> (n));};

    // Buffer the data if there is enough space.
    //
    size_t an (epptr () - pptr ()); // Amount of free space in the buffer.
    if (n <= an)
    {
      memcpy (pptr (), s, n);
      advance (n);
      return n;
    }

    size_t bn (pptr () - pbase ()); // Buffered data size.

#ifndef _WIN32

    ssize_t r;
    if (bn > 0)
    {
      // Write both buffered and new data with a single system call.
      //
      iovec iov[2] = {{pbase (), bn}, {const_cast<char*> (s), n}};
      r = writev (fd_.get (), iov, 2);
    }
    else
      r = write (fd_.get (), s, n);

    if (r == -1)
      throw_generic_ios_failure (errno);

    size_t m (static_cast<size_t> (r));
    off_ += m;

    // If the buffered data wasn't fully written then move the unwritten part
    // to the beginning of the buffer.
    //
    if (m < bn)
    {
      memmove (pbase (), pbase () + m, bn - m);
      pbump (-static_cast<int> (m)); // Note that pbump() accepts negatives.
      return 0;
    }

    setp (buf_, buf_ + sizeof (buf_) - 1);
    return m - bn;

#else

    // On Windows there is no writev() available so sometimes we will make two
    // system calls. Fill and flush the buffer, then try to fit the data tail
    // into the empty buffer. If the data tail is too long then just write it
    // to the file and keep the buffer empty.
    //
    // We will end up with two _write() calls if the total data size to be
    // written exceeds double the buffer size. In this case the buffer filling
    // is redundant so let's pretend there is no free space in the buffer, and
    // so buffered and new data will be written separatelly.
    //
    if (bn + n > 2 * (bn + an))
      an = 0;
    else
    {
      memcpy (pptr (), s, an);
      advance (an);
    }

    // Flush the buffer.
    //
    size_t wn (bn + an);
    int r (wn > 0 ? write (fd_.get (), buf_, wn) : 0);

    if (r == -1)
      throw_generic_ios_failure (errno);

    size_t m (static_cast<size_t> (r));
    off_ += m;

    // If the buffered data wasn't fully written then move the unwritten part
    // to the beginning of the buffer.
    //
    if (m < wn)
    {
      memmove (pbase (), pbase () + m, wn - m);
      pbump (-static_cast<int> (m)); // Note that pbump() accepts negatives.
      return m < bn ? 0 : m - bn;
    }

    setp (buf_, buf_ + sizeof (buf_) - 1);

    // Now 'an' holds the size of the data portion written as a part of the
    // buffer flush.
    //
    s += an;
    n -= an;

    // Buffer the data tail if it fits the buffer.
    //
    if (n <= static_cast<size_t> (epptr () - pbase ()))
    {
      memcpy (pbase (), s, n);
      advance (n);
      return sn;
    }

    // The data tail doesn't fit the buffer so write it to the file.
    //
    r = write (fd_.get (), s, n);

    if (r == -1)
      throw_generic_ios_failure (errno);

    off_ += r;

    return an + r;
#endif
  }

  inline static bool
  flag (fdstream_mode m, fdstream_mode flag)
  {
    return (m & flag) == flag;
  }

  inline static auto_fd
  mode (auto_fd fd, fdstream_mode m)
  {
    if (fd.get () >= 0 &&
        (flag (m, fdstream_mode::text) ||
         flag (m, fdstream_mode::binary) ||
         flag (m, fdstream_mode::blocking) ||
         flag (m, fdstream_mode::non_blocking)))
      fdmode (fd.get (), m);

    return fd;
  }

  // fdstream_base
  //
  fdstream_base::
  fdstream_base (auto_fd&& fd, fdstream_mode m)
      : fdstream_base (mode (move (fd), m)) // Delegate.
  {
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
      throw_generic_ios_failure (EIO, "getline failure");

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
    // C++17 deprecated uncaught_exception() so use uncaught_exceptions() if
    // available.
    //
#ifdef __cpp_lib_uncaught_exceptions
    assert (!is_open () || !good () || uncaught_exceptions () != 0);
#else
    assert (!is_open () || !good () || uncaught_exception ());
#endif
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
  auto_fd
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

    // Unlike other platforms, FreeBSD allows opening a directory as a file
    // which will cause all kinds of problems upstream (e.g., cpfile()). So we
    // detect and diagnose this.
    //
#ifdef __FreeBSD__
    {
      struct stat s;
      if (stat (f, &s) == 0 && S_ISDIR (s.st_mode))
        throw_generic_ios_failure (EISDIR);
    }
#endif

    int fd (open (f, of | O_CLOEXEC, pf));

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
    // existing file. However it does if we pass them (reproduced on Windows
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
        throw_generic_ios_failure (EEXIST);

      of &= ~_O_CREAT;
      pass_perm = false;
    }

    // Make sure the file descriptor is not inheritable by default.
    //
    of |= _O_NOINHERIT;

    int fd (pass_perm
            ? _sopen (f, of, _SH_DENYNO, pf)
            : _sopen (f, of, _SH_DENYNO));

#endif

    if (fd == -1)
      throw_generic_ios_failure (errno);

    if (mode (fdopen_mode::at_end))
    {
#ifndef _WIN32
      bool r (lseek (fd, 0, SEEK_END) != static_cast<off_t> (-1));
#else
      bool r (_lseek (fd, 0, SEEK_END) != -1);
#endif

      // Note that in the case of an error we don't delete the newly created
      // file as we have no indication if it is a new one.
      //
      if (!r)
      {
        int e (errno);
        fdclose (fd); // Doesn't throw, but can change errno.
        throw_generic_ios_failure (e);
      }
    }

    return auto_fd (fd);
  }

#ifndef _WIN32

  auto_fd
  fddup (int fd)
  {
    // dup() doesn't copy FD_CLOEXEC flag, so we need to do it ourselves. Note
    // that the new descriptor can leak into child processes before we copy the
    // flag. To prevent this we will acquire the process_spawn_mutex (see
    // process-details header) prior to duplicating the descriptor. Also note
    // there is dup3() (available on Linux and FreeBSD but not on Max OS) that
    // takes flags, but it's usage tends to be hairy (need to preopen a dummy
    // file descriptor to pass as a second argument).
    //
    auto dup = [fd] () -> auto_fd
    {
      auto_fd nfd (::dup (fd));
      if (nfd.get () == -1)
        throw_generic_ios_failure (errno);

      return nfd;
    };

    int f (fcntl (fd, F_GETFD));
    if (f == -1)
      throw_generic_ios_failure (errno);

    // If the source descriptor has no FD_CLOEXEC flag set then no flag copy is
    // required (as the duplicate will have no flag by default).
    //
    if ((f & FD_CLOEXEC) == 0)
      return dup ();

    slock l (process_spawn_mutex);
    auto_fd nfd (dup ());

    f = fcntl (nfd.get (), F_GETFD);
    if (f == -1 || fcntl (nfd.get (), F_SETFD, f | FD_CLOEXEC) == -1)
      throw_generic_ios_failure (errno);

    return nfd;
  }

  bool
  fdclose (int fd) noexcept
  {
    return close (fd) == 0;
  }

  auto_fd
  fdnull ()
  {
    int fd (open ("/dev/null", O_RDWR | O_CLOEXEC));

    if (fd == -1)
      throw_generic_ios_failure (errno);

    return auto_fd (fd);
  }

  fdstream_mode
  fdmode (int fd, fdstream_mode m)
  {
    int flags (fcntl (fd, F_GETFL));

    if (flags == -1)
      throw_generic_ios_failure (errno);

    if (flag (m, fdstream_mode::blocking) ||
        flag (m, fdstream_mode::non_blocking))
    {
      m &= fdstream_mode::blocking | fdstream_mode::non_blocking;

      // Should be exactly one blocking mode flag specified.
      //
      if (m != fdstream_mode::blocking && m != fdstream_mode::non_blocking)
        throw invalid_argument ("invalid blocking mode");

      int new_flags (
        m == fdstream_mode::non_blocking
        ? flags | O_NONBLOCK
        : flags & ~O_NONBLOCK);

      if (fcntl (fd, F_SETFL, new_flags) == -1)
        throw_generic_ios_failure (errno);
    }

    return fdstream_mode::binary |
      ((flags & O_NONBLOCK) == O_NONBLOCK
       ? fdstream_mode::non_blocking
       : fdstream_mode::blocking);
  }

  int
  stdin_fd ()
  {
    return STDIN_FILENO;
  }

  int
  stdout_fd ()
  {
    return STDOUT_FILENO;
  }

  int
  stderr_fd ()
  {
    return STDERR_FILENO;
  }

  fdpipe
  fdopen_pipe (fdopen_mode m)
  {
    assert (m == fdopen_mode::none || m == fdopen_mode::binary);

    // Note that the pipe file descriptors can leak into child processes before
    // we set FD_CLOEXEC flag for them. To prevent this we will acquire the
    // process_spawn_mutex (see process-details header) prior to creating the
    // pipe. Also note there is pipe2() (available on Linux and FreeBSD but not
    // on Max OS) that takes flags.
    //
    slock l (process_spawn_mutex);

    int pd[2];
    if (pipe (pd) == -1)
      throw_generic_ios_failure (errno);

    fdpipe r {auto_fd (pd[0]), auto_fd (pd[1])};

    for (size_t i (0); i < 2; ++i)
    {
      int f (fcntl (pd[i], F_GETFD));
      if (f == -1 || fcntl (pd[i], F_SETFD, f | FD_CLOEXEC) == -1)
        throw_generic_ios_failure (errno);
    }

    return r;
  }

  bool
  fdterm (int fd)
  {
    int r (isatty (fd));

    if (r == 1)
      return true;

    // POSIX specifies ENOTTY errno code for an indication that the descriptor
    // doesn't refer to a terminal. However, both Linux and FreeBSD man pages
    // also mention EINVAL for this case.
    //
    assert (r == 0);

    if (errno == ENOTTY || errno == EINVAL)
      return false;

    throw_generic_ios_failure (errno);
  }

#else

  auto_fd
  fddup (int fd)
  {
    // _dup() doesn't copy _O_NOINHERIT flag, so we need to do it ourselves.
    // Note that the new descriptor can leak into child processes before we
    // copy the flag. To prevent this we will acquire the process_spawn_mutex
    // (see process-details header) prior to duplicating the descriptor.
    //
    // We can not ammend file descriptors directly (nor obtain the flag value),
    // so need to resolve them to Windows HANDLE first. Such handles are closed
    // either when CloseHandle() is called for them or when _close() is called
    // for the associated file descriptors. Make sure that either the original
    // file descriptor or the resulting HANDLE is closed but not both of them.
    //
    auto handle = [] (int fd) -> HANDLE
    {
      HANDLE h (reinterpret_cast<HANDLE> (_get_osfhandle (fd)));
      if (h == INVALID_HANDLE_VALUE)
        throw_generic_ios_failure (errno); // EBADF (POSIX value).

      return h;
    };

    auto dup = [fd] () -> auto_fd
    {
      auto_fd nfd (_dup (fd));
      if (nfd.get () == -1)
        throw_generic_ios_failure (errno);

      return nfd;
    };

    DWORD f;
    if (!GetHandleInformation (handle (fd), &f))
      throw_system_ios_failure (GetLastError ());

    // If the source handle is inheritable then no flag copy is required (as
    // the duplicate handle will be inheritable by default).
    //
    if (f & HANDLE_FLAG_INHERIT)
      return dup ();

    slock l (process_spawn_mutex);

    auto_fd nfd (dup ());
    if (!SetHandleInformation (handle (nfd.get ()), HANDLE_FLAG_INHERIT, 0))
      throw_system_ios_failure (GetLastError ());

    return nfd;
  }

  bool
  fdclose (int fd) noexcept
  {
    return _close (fd) == 0;
  }

  auto_fd
  fdnull (bool temp)
  {
    // No need to translate \r\n before sending it to void.
    //
    if (!temp)
    {
      int fd (_sopen ("nul", _O_RDWR | _O_BINARY | _O_NOINHERIT, _SH_DENYNO));

      if (fd == -1)
        throw_generic_ios_failure (errno);

      return auto_fd (fd);
    }

    try
    {
      // We could probably implement a Windows-specific version of getting
      // the temporary file that avoid any allocations and exceptions.
      //
      path p (path::temp_path ("null")); // Can throw.
      int fd (_sopen (p.string ().c_str (),
                      (_O_CREAT       |
                       _O_RDWR        |
                       _O_BINARY      | // Don't translate.
                       _O_TEMPORARY   | // Remove on close.
                       _O_SHORT_LIVED | // Don't flush to disk.
                       _O_NOINHERIT),   // Don't inherit by child process.
                      _SH_DENYNO,
                      _S_IREAD | _S_IWRITE));

      if (fd == -1)
        throw_generic_ios_failure (errno);

      return auto_fd (fd);
    }
    catch (const bad_alloc&)
    {
      throw_generic_ios_failure (ENOMEM);
    }
    catch (const system_error& e)
    {
      // Make sure that the error denotes errno portable code.
      //
      assert (e.code ().category () == generic_category ());

      throw_generic_ios_failure (e.code ().value ());
    }
  }

  fdstream_mode
  fdmode (int fd, fdstream_mode m)
  {
    m &= fdstream_mode::text | fdstream_mode::binary;

    // Should be exactly one translation flag specified.
    //
    // It would have been natural not to change translation mode if none of
    // text or binary flags are passed. Unfortunatelly there is no (easy) way
    // to obtain the current mode for the file descriptor without setting a
    // new one. This is why not specifying one of the modes is an error.
    //
    if (m != fdstream_mode::binary && m != fdstream_mode::text)
      throw invalid_argument ("invalid translation mode");

    // Note that _setmode() preserves the _O_NOINHERIT flag value.
    //
    int r (_setmode (fd, m == fdstream_mode::binary ? _O_BINARY : _O_TEXT));
    if (r == -1)
      throw_generic_ios_failure (errno); // EBADF or EINVAL (POSIX values).

    return fdstream_mode::blocking |
      ((r & _O_BINARY) == _O_BINARY
       ? fdstream_mode::binary
       : fdstream_mode::text);
  }

  int
  stdin_fd ()
  {
    int fd (_fileno (stdin));
    if (fd == -1)
      throw_generic_ios_failure (errno);

    return fd;
  }

  int
  stdout_fd ()
  {
    int fd (_fileno (stdout));
    if (fd == -1)
      throw_generic_ios_failure (errno);

    return fd;
  }

  int
  stderr_fd ()
  {
    int fd (_fileno (stderr));
    if (fd == -1)
      throw_generic_ios_failure (errno);

    return fd;
  }

  fdpipe
  fdopen_pipe (fdopen_mode m)
  {
    assert (m == fdopen_mode::none || m == fdopen_mode::binary);

    int pd[2];
    if (_pipe (
          pd,
          64 * 1024, // Set buffer size to 64K.
          _O_NOINHERIT | (m == fdopen_mode::none ? _O_TEXT : _O_BINARY)) == -1)
      throw_generic_ios_failure (errno);

    return {auto_fd (pd[0]), auto_fd (pd[1])};
  }

  bool
  fdterm (int fd)
  {
    // Resolve file descriptor to HANDLE. Note that the handle is closed either
    // when CloseHandle() is called for it or when _close() is called for the
    // associated file descriptor. So we don't need to close it.
    //
    HANDLE h (reinterpret_cast<HANDLE> (_get_osfhandle (fd)));
    if (h == INVALID_HANDLE_VALUE)
      throw_generic_ios_failure (errno);

    // Obtain the descriptor type.
    //
    DWORD t (GetFileType (h));
    DWORD e;

    if (t == FILE_TYPE_UNKNOWN && (e = GetLastError ()) != 0)
      throw_system_ios_failure (e);

    if (t == FILE_TYPE_CHAR) // Terminal.
      return true;

    if (t != FILE_TYPE_PIPE) // Pipe still can be a terminal (see below).
      return false;

    // MSYS2 terminal file descriptor has the pipe type. To distinguish it
    // from other pipes we will try to obtain the associated file name and
    // heuristically decide if it is a terminal or not. If we fail to obtain
    // the name (for any reason), then we consider the descriptor as not
    // referring to a terminal.
    //
    // Note that the API we need to use is only available starting with
    // Windows Vista/Server 2008. To allow programs linked to libbutl to run
    // on earlier Windows versions we will link the API in run-time and will
    // fallback to the non-terminal descriptor type on failure. We also need
    // to partially reproduce the original API types.
    //
    HMODULE kh (GetModuleHandle ("kernel32.dll"));
    if (kh == nullptr)
      return false;

    // The original type is FILE_INFO_BY_HANDLE_CLASS enum.
    //
    enum file_info
    {
      file_name = 2
    };

    using func = BOOL (*) (HANDLE, file_info, LPVOID, DWORD);

    func f (reinterpret_cast<func> (
      GetProcAddress (kh, "GetFileInformationByHandleEx")));

    if (f == nullptr)
      return false;

    // The original type is FILE_NAME_INFO structure.
    //
    struct
    {
      DWORD length;
      wchar_t name[_MAX_PATH + 1];
    } fn;

    // Reserve space for the trailing NULL character.
    //
    if (!f (h, file_name, &fn, sizeof (fn) - sizeof (wchar_t)))
      return false;

    // Add the trailing NULL character. Sounds strange, but the name length is
    // expressed in bytes.
    //
    fn.name[fn.length / sizeof (wchar_t)] = L'\0';

    // The MSYS2 terminal descriptor file name looks like this:
    //
    // \msys-dd50a72ab4668b33-pty0-to-master
    //
    // We will recognize it by the '\msys-' prefix and the presence of the
    // '-ptyN' entry.
    //
    if (wcsncmp (fn.name, L"\\msys-", 6) == 0)
    {
      const wchar_t* e (wcsstr (fn.name, L"-pty"));

      return e != nullptr && e[4] >= L'0' && e[4] <= L'9' &&
        (e[5] == L'-' || e[5] == L'\0');
    }

    return false;
  }
#endif
}
