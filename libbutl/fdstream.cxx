// file      : libbutl/fdstream.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/fdstream.hxx>

#include <errno.h> // errno, E*

#ifndef _WIN32
#  include <fcntl.h>      // open(), O_*, fcntl()
#  include <unistd.h>     // close(), read(), write(), lseek(), dup(), pipe(),
                          // ftruncate(), isatty(), ssize_t, STD*_FILENO
#  include <sys/uio.h>    // writev(), iovec
#  include <sys/stat.h>   // stat(), fstat(), S_I*
#  include <sys/time.h>   // timeval
#  include <sys/types.h>  // stat, off_t
#  include <sys/select.h>
#else
#  include <libbutl/win32-utility.hxx>

#  ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x04
#  endif

#  include <io.h>        // _close(), _read(), _write(), _setmode(), _sopen(),
                         // _lseek(), _dup(), _pipe(), _chsize_s,
                         // _get_osfhandle()
#  include <share.h>     // _SH_DENYNO
#  include <stdio.h>     // _fileno(), stdin, stdout, stderr, SEEK_*
#  include <fcntl.h>     // _O_*
#  include <sys/types.h> // _stat
#  include <sys/stat.h>  // fstat(), S_I*

#  ifdef _MSC_VER // Unlikely to be fixed in newer versions.
#    define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#    define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#    define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#  endif

#  include <wchar.h> // wcsncmp(), wcsstr()

#  include <thread>    // this_thread::yield()
#  include <algorithm> // count()
#endif

#include <ios>          // ios_base::openmode, ios_base::failure
#include <new>          // bad_alloc
#include <limits>       // numeric_limits
#include <cassert>
#include <cstring>      // memcpy(), memmove(), memchr(), strcmp()
#include <cstdlib>      // getenv()
#include <iostream>     // cin, cout
#include <exception>    // uncaught_exception[s]()
#include <stdexcept>    // invalid_argument
#include <system_error>

#include <libbutl/ft/exception.hxx>    // uncaught_exceptions
#include <libbutl/process-details.hxx>

#include <libbutl/utility.hxx>   // throw_*_ios_failure(), function_cast()
#include <libbutl/timestamp.hxx>

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

#ifdef _WIN32
  // Resolve a file descriptor to HANDLE. On the underlying OS error throw
  // ios::failure or return INVALID_HANDLE_VALUE if ignore_error is true.
  //
  // Note that such a handle is closed either when CloseHandle() is called on
  // the HANDLE or when _close() is called on the associated file descriptor.
  // So make sure that either the original file descriptor or the resulting
  // HANDLE is closed but not both of them.
  //
  static HANDLE
  fd_to_handle (int fd, bool ignore_error = false)
  {
    HANDLE h (reinterpret_cast<HANDLE> (_get_osfhandle (fd)));
    if (h == INVALID_HANDLE_VALUE && !ignore_error)
      throw_generic_ios_failure (errno); // EBADF (POSIX value).

    return h;
  }

  // Return the pipe handle and blocking mode or nullopt if the file
  // descriptor is not a pipe. On the underlying OS error throw ios::failure
  // or return nullopt if ignore_error is true.
  //
  struct handle_mode
  {
    HANDLE handle;
    fdstream_mode mode;
  };

  static optional<handle_mode>
  pipe_mode (int fd, bool ignore_error = false)
  {
    // We don't need to close it (see fd_to_handle()).
    //
    HANDLE h (fd_to_handle (fd, ignore_error));
    if (h == INVALID_HANDLE_VALUE)
      return nullopt;

    // If we fail to obtain a pipe state for a valid handle, then we assume
    // that this is not a pipe.
    //
    // Note that for other handle types GetLastError() may return different
    // error codes depending on the type and if we run natively or under Wine.
    //
    DWORD m;
    if (!GetNamedPipeHandleState (h,
                                  &m,
                                  nullptr /* lpCurInstances */,
                                  nullptr /* lpMaxCollectionCount */,
                                  nullptr /* lpCollectDataTimeout */,
                                  nullptr /* lpUserName */,
                                  0       /* nMaxUserNameSize */))
      return nullopt;

    return handle_mode {h,
                        (m & PIPE_NOWAIT) != 0
                        ? fdstream_mode::non_blocking
                        : fdstream_mode::blocking};
  }
#endif

  // fdstreambuf
  //
  // Return true if the file descriptor is in the non-blocking mode. Throw
  // ios::failure on the underlying OS error.
  //
  static bool
  non_blocking (int fd)
  {
#ifndef _WIN32
    int flags (fcntl (fd, F_GETFL));

    if (flags == -1)
      throw_generic_ios_failure (errno);

    return (flags & O_NONBLOCK) == O_NONBLOCK;
#else
    optional<handle_mode> m (pipe_mode (fd));
    return m && m->mode == fdstream_mode::non_blocking;
#endif
  }

  void fdstreambuf::
  open (auto_fd&& fd, uint64_t pos)
  {
    close ();

    non_blocking_ = non_blocking (fd.get ());

    setg (buf_, buf_, buf_);
    setp (buf_, buf_ + sizeof (buf_) - 1); // Keep space for overflow's char.
    off_ = pos;
    fd_ = move (fd);
  }

  bool fdstreambuf::
  blocking (bool m)
  {
    // Verify that the file descriptor is open.
    //
    if (!is_open ())
      throw_generic_ios_failure (EBADF); // POSIX value.

    // Bail out if we are already in the requested mode.
    //
    if (m != non_blocking_)
      return m;

    // Change the mode.
    //
    fdmode (fd (), m ? fdstream_mode::blocking : fdstream_mode::non_blocking);

    // Get the effective file descriptor blocking mode (see fdmode() for
    // details).
    //
    non_blocking_ = non_blocking (fd ());
    return !m;
  }

  streamsize fdstreambuf::
  showmanyc ()
  {
    if (!is_open ())
      return -1;

    streamsize n (egptr () - gptr ());

    if (n > 0)
      return n;

    if (non_blocking_)
    {
      streamsize n (fdread (fd_.get (), buf_, sizeof (buf_)));

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

    return 0;
  }

  fdstreambuf::int_type fdstreambuf::
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

  bool fdstreambuf::
  load ()
  {
    // Doesn't handle blocking mode and so should not be called.
    //
    assert (!non_blocking_);

    streamsize n (fdread (fd_.get (), buf_, sizeof (buf_)));

    if (n == -1)
      throw_generic_ios_failure (errno);

    setg (buf_, buf_, buf_ + n);
    off_ += n;
    return n != 0;
  }

  void fdstreambuf::
  seekg (uint64_t off)
  {
    // In the future we may implement the blocking behavior for a non-blocking
    // file descriptor.
    //
    if (non_blocking_)
      throw_generic_ios_failure (ENOTSUP);

    // The plan is to rewind to the beginning of the stream, read the
    // requested number of characters and reset the get area, so it will be
    // filled from scratch on the next read from the stream.
    //
    fdseek (fd_.get (), 0, fdseek_mode::set);

    for (uint64_t n (off); n != 0; )
    {
      size_t m (n > sizeof (buf_) ? sizeof (buf_) : static_cast<size_t> (n));
      streamsize r (fdread (fd_.get (), buf_, m));

      if (r == -1)
        throw_generic_ios_failure (errno);

      // Fail if trying to seek beyond the end of the stream.
      //
      if (r == 0)
        throw_generic_ios_failure (EINVAL);

      n -= r;
    }

    off_ = off;
    setg (buf_, buf_, buf_);
  }

  fdstreambuf::int_type fdstreambuf::
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

  int fdstreambuf::
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

  bool fdstreambuf::
  save ()
  {
    size_t n (pptr () - pbase ());

    if (n != 0)
    {
      // Note that for MinGW GCC (5.2.0) _write() returns 0 for a file
      // descriptor opened for read-only access (while -1 with errno EBADF is
      // expected). This is in contrast with VC's _write() and POSIX's write().
      //
      auto m (fdwrite (fd_.get (), buf_, n));

      if (m == -1)
        throw_generic_ios_failure (errno);

      off_ += m;

      if (n != static_cast<size_t> (m))
        return false;

      setp (buf_, buf_ + sizeof (buf_) - 1);
    }

    return true;
  }

  streamsize fdstreambuf::
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
      assert (s != nullptr || n == 0);

      // Note that the memcpy() function behavior is undefined if either of
      // pointers is NULL, even if the bytes count is zero.
      //
      if (s != nullptr)
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
      assert (s != nullptr || an == 0);

      // The source can not be NULL (see above for details).
      //
      if (s != nullptr)
        memcpy (pptr (), s, an);

      advance (an);
    }

    // Flush the buffer.
    //
    size_t wn (bn + an);
    streamsize r (wn > 0 ? fdwrite (fd_.get (), buf_, wn) : 0);

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
      assert (s != nullptr || n == 0);

      // The source can not be NULL (see above for details).
      //
      if (s != nullptr)
        memcpy (pbase (), s, n);

      advance (n);
      return sn;
    }

    // The data tail doesn't fit the buffer so write it to the file.
    //
    r = fdwrite (fd_.get (), s, n);

    if (r == -1)
      throw_generic_ios_failure (errno);

    off_ += r;

    return an + r;
#endif
  }

  // Common call chains:
  //
  // - basic_ostream::seekp(pos)                    ->
  //     basic_streambuf::pubseekpos(pos, ios::out) ->
  //       fdstreambuf::seekpos(pos, ios::out)
  //
  // - basic_istream::seekg(pos)                   ->
  //     basic_streambuf::pubseekpos(pos, ios::in) ->
  //       fdstreambuf::seekpos(pos, ios::in)
  //
  fdstreambuf::pos_type fdstreambuf::
  seekpos (pos_type pos, ios_base::openmode which)
  {
    // Note that the position type provides an explicit conversion to the
    // numeric offset type (see std::fpos for details). The position state is
    // disregarded in this case, which is ok since we don't mess with the
    // multibyte character conversions.
    //
    return seekoff (static_cast<off_type> (pos), ios_base::beg, which);
  }

  // Common call chains:
  //
  // - basic_ostream::seekp(off, dir)                    ->
  //     basic_streambuf::pubseekoff(off, dir, ios::out) ->
  //       fdstreambuf::seekoff(off, dir, ios::out)
  //
  // - basic_ostream::tellp()                               ->
  //     basic_streambuf::pubseekoff(0, ios::cur, ios::out) ->
  //       fdstreambuf::seekoff(0, ios::cur, ios::out)
  //
  // - basic_istream::seekg(off, dir)                   ->
  //     basic_streambuf::pubseekoff(off, dir, ios::in) ->
  //       fdstreambuf::seekoff(off, dir, ios::in)
  //
  // - basic_istream::tellg()                              ->
  //     basic_streambuf::pubseekoff(0, ios::cur, ios::in) ->
  //       fdstreambuf::seekoff(0, ios::cur, ios::in)
  //
  fdstreambuf::pos_type fdstreambuf::
  seekoff (off_type off, ios_base::seekdir dir, ios_base::openmode which)
  {
    // The seekoff() function interface doesn't support the non-blocking
    // semantics since being unable to serialize the character in write mode
    // is supposed to be an error. Also the non-blocking mode is likely to be
    // used for non-seekable file descriptors (pipes, etc.). In the future we
    // may implement the blocking behavior for a non-blocking file descriptor.
    //
    if (non_blocking_)
      throw_generic_ios_failure (ENOTSUP);

    // Translate ios_base::seekdir value to fdseek_mode. Note:
    // ios_base::seekdir is an implementation-defined type and is not
    // necessarily a enum.
    //
    fdseek_mode m (fdseek_mode::set);
    switch (dir)
    {
    case ios_base::beg: m = fdseek_mode::set; break;
    case ios_base::cur: m = fdseek_mode::cur; break;
    case ios_base::end: m = fdseek_mode::end; break;
    default: assert (false);
    }

    // Prior to fdseek() call we will flush the buffer for the write mode,
    // reset the get area for the read mode, and fail otherwise. Note that we
    // don't support the read/write mode.
    //
    // Note that the return (position) type is implicitly constructible from
    // the numeric offset type (see std::fpos for details).
    //
    switch (which)
    {
    case ios_base::out:
      {
        // Fail if unable to fully flush the buffer (for example, because the
        // device is full).
        //
        if (!save ())
          return static_cast<off_type> (-1);

        break;
      }
    case ios_base::in:
      {
        // We may have unread data in the get area and need to subtract its
        // size from the offset if we seek from the current position.
        //
        if (dir == ios_base::cur)
        {
          off_type n (egptr () - gptr ()); // Get area size.

#ifdef _WIN32
          // Note that on Windows, when reading in the text mode, newline
          // characters are translated from the CRLF character sequences.
          // Thus, in this mode, we also need to subtract the number of
          // newlines in the get area from the offset.
          //
          // Note that this approach only works for "canonical" Windows text
          // files. Specifically, if there are newlines not preceded with the
          // CR character then we may end up in the wrong place. It seems that
          // there is no reasonable solution for this problem, and neither of
          // the MSVC's or MinGW's std::ifstream implementations handle this
          // case properly.
          //

          // The only way to query the current file descriptor mode is to
          // reset it and use the result (see fdmode() for details).
          //
          fdstream_mode fm (fdmode (fd_.get (), fdstream_mode::text));

          // Note: the fdstream_mode::blocking flag is also set.
          //
          if ((fm & fdstream_mode::text) == fdstream_mode::text)
            n += count (gptr (), egptr (), '\n');
          else
            fdmode (fd_.get (), fm); // Restore the mode if it was changed.
#endif

          // Note that ifdstream::tellg() implicitly calls seekoff(0,ios::cur)
          // (see above). Let's not reset the get area for such noop seeks.
          //
          if (off == 0)
            return static_cast<off_type> (
              fdseek (fd_.get (), 0, fdseek_mode::cur) - n);

          off -= n;
        }

        // Reset the get area.
        //
        setg (buf_, buf_, buf_);
        break;
      }
    default: return static_cast<off_type> (-1);
    }

    // Note that on Windows in the text mode the logical offset (number of
    // read/written bytes) is likely to be screwed up due to newlines
    // translation (see above).
    //
    off_ = fdseek (fd_.get (), off, m);

    return static_cast<off_type> (off_);
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
        (flag (m, fdstream_mode::text)     ||
         flag (m, fdstream_mode::binary)   ||
         flag (m, fdstream_mode::blocking) ||
         flag (m, fdstream_mode::non_blocking)))
      fdmode (fd.get (), m);

    return fd;
  }

  // fdstream_base
  //
  fdstream_base::
  fdstream_base (auto_fd&& fd, fdstream_mode m, std::uint64_t pos)
      : fdstream_base (mode (move (fd), m), pos)
  {
  }

  static fdopen_mode
  translate_mode (ios_base::openmode m)
  {
    using openmode = ios_base::openmode;

    const openmode in    (ios_base::in);
    const openmode out   (ios_base::out);
    const openmode app   (ios_base::app);
    const openmode bin   (ios_base::binary);
    const openmode trunc (ios_base::trunc);
    const openmode ate   (ios_base::ate);

    const fdopen_mode fd_in     (fdopen_mode::in);
    const fdopen_mode fd_out    (fdopen_mode::out);
    const fdopen_mode fd_inout  (fdopen_mode::in | fdopen_mode::out);
    const fdopen_mode fd_app    (fdopen_mode::append);
    const fdopen_mode fd_trunc  (fdopen_mode::truncate);
    const fdopen_mode fd_create (fdopen_mode::create);
    const fdopen_mode fd_bin    (fdopen_mode::binary);
    const fdopen_mode fd_ate    (fdopen_mode::at_end);

    // Note: cannot use switch due to warnings (value no in enumerator).
    //
    openmode sm (m & ~(ate | bin));
    fdopen_mode r (
      sm == (in)                                ? fd_in                       :
      sm == (out)          || sm == (out|trunc) ? fd_out|fd_trunc|fd_create   :
      sm == (app)          || sm == (out|app)   ? fd_out|fd_app|fd_create     :
      sm == (out|in)                            ? fd_inout                    :
      sm == (out|in|trunc)                      ? fd_inout|fd_trunc|fd_create :
      sm == (out|in|app)   || sm == (in|app)    ? fd_inout|fd_app|fd_create   :
      fdopen_mode::none);

    if (r == fdopen_mode::none)
      throw invalid_argument ("invalid open mode");

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
      : ifdstream (f, translate_mode (m | in), e)
  {
  }

  ifdstream::
  ifdstream (const char* f, fdopen_mode m, iostate e)
      : ifdstream (fdopen (f,
                           // If fdopen_mode::in is not specified, then
                           // emulate the ios::in semantics.
                           //
                           (m & fdopen_mode::in) == fdopen_mode::in
                           ? m
                           : m | translate_mode (in)),
                   e)
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

      // The ignore() function doesn't support the non-blocking semantics as
      // it extracts and discards characters until the limit is reached or EOF
      // is encountered. That's why we switch to the blocking mode.
      //
      // It's highly unlikely to be unable to set the blocking mode for a
      // valid fd. If, however, that happens, we can't do much about it.
      //
      try
      {
        buf_.blocking (true);
        ignore (numeric_limits<streamsize>::max ());
      }
      catch (const ios_base::failure&) {}
    }

    // Underlying file descriptor is closed by fdstreambuf dtor with errors
    // (if any) being ignored.
  }

  void ifdstream::
  open (const char* f, openmode m)
  {
    open (f, translate_mode (m | in));
  }

  void ifdstream::
  open (const char* f, fdopen_mode m)
  {
    open (fdopen (f,
                  (m & fdopen_mode::in) == fdopen_mode::in
                  ? m
                  : m | translate_mode (in)));
  }

  void ifdstream::
  open (auto_fd&& fd, fdstream_mode m, std::uint64_t pos)
  {
    open (mode (std::move (fd), m), pos);
    skip_ = (m & fdstream_mode::skip) == fdstream_mode::skip;
  }

  void ifdstream::
  close ()
  {
    if (skip_ && is_open () && good ())
    {
      // The ignore() function doesn't support the non-blocking semantics (see
      // above).
      //
      buf_.blocking (true);
      ignore (numeric_limits<streamsize>::max ());
    }

    buf_.close ();
  }

  ifdstream&
  getline (ifdstream& is, string& l, char delim)
  {
    ifdstream::iostate eb (is.exceptions ());
    assert (eb & ifdstream::badbit);

    // Amend the exception mask to prevent exceptions being thrown by the C++
    // IO runtime to avoid incompatibility issues due to ios_base::failure ABI
    // fiasco (#66145). We will not restore the mask when ios_base::failure is
    // thrown by fdstreambuf since there is no way to "silently" restore it if
    // the corresponding bits are in the error state without the exceptions()
    // call throwing ios_base::failure. Not restoring exception mask on
    // throwing because of badbit should probably be ok since the stream is no
    // longer usable.
    //
    if (eb != ifdstream::badbit)
      is.exceptions (ifdstream::badbit);

    std::getline (is, l, delim);

    // Throw if any of the newly set bits are present in the exception mask.
    //
    if ((is.rdstate () & eb) != ifdstream::goodbit)
      throw_generic_ios_failure (EIO, "getline failure");

    if (eb != ifdstream::badbit)
      is.exceptions (eb); // Restore exception mask.

    return is;
  }

  bool
  getline_non_blocking (ifdstream& is, string& l, char delim)
  {
    assert (!is.blocking () && (is.exceptions () & ifdstream::badbit) != 0);

    fdstreambuf& sb (*static_cast<fdstreambuf*> (is.rdbuf ()));

    // Read until blocked (0), EOF (-1) or encounter the delimiter.
    //
    // Note that here we reasonably assume that any failure in in_avail()
    // will lead to badbit and thus an exception (see showmanyc()).
    //
    streamsize s;
    while ((s = sb.in_avail ()) > 0)
    {
      const char* p (sb.gptr ());
      size_t n (sb.egptr () - p);

      const char* e (static_cast<const char*> (memchr (p, delim, n)));
      if (e != nullptr)
        n = e - p;

      l.append (p, n);

      // Note: consume the delimiter if found.
      //
      sb.gbump (static_cast<int> (n + (e != nullptr ? 1 : 0)));

      if (e != nullptr)
        break;
    }

    // Here s can be:
    //
    // -1 -- EOF.
    //  0 -- blocked before encountering delimiter/EOF.
    // >0 -- encountered the delimiter.
    //
    if (s == -1)
    {
      is.setstate (ifdstream::eofbit);

      // If we couldn't extract anything, not even the delimiter, then this is
      // a failure per the getline() interface.
      //
      if (l.empty ())
        is.setstate (ifdstream::failbit);
    }

    return s != 0;
  }

  // ofdstream
  //
  ofdstream::
  ofdstream (const char* f, openmode m, iostate e)
      : ofdstream (f, translate_mode (m | out), e)
  {
  }

  ofdstream::
  ofdstream (const char* f, fdopen_mode m, iostate e)
      : ofdstream (fdopen (f,
                           // If fdopen_mode::out is not specified, then
                           // emulate the ios::out semantics.
                           //
                           (m & fdopen_mode::out) == fdopen_mode::out
                           ? m
                           : m | translate_mode (out)),
                   e)
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
    open (fdopen (f,
                  (m & fdopen_mode::out) == fdopen_mode::out
                  ? m
                  : m | translate_mode (out)));
  }

  istream&
  open_file_or_stdin (path_name& pn, ifdstream& ifs)
  {
    assert (pn.path != nullptr);

    if (pn.path->string () != "-")
    {
      ifs.open (*pn.path);
      return ifs;
    }
    else
    {
      cin.exceptions (ifs.exceptions ());
      if (!pn.name)
        pn.name = "<stdin>";
      return cin;
    }
  }

  ostream&
  open_file_or_stdout (path_name& pn, ofdstream& ofs)
  {
    assert (pn.path != nullptr);

    if (pn.path->string () != "-")
    {
      ofs.open (*pn.path);
      return ofs;
    }
    else
    {
      cout.exceptions (ofs.exceptions ());
      if (!pn.name)
        pn.name = "<stdout>";
      return cout;
    }
  }

  // fd*() functions
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

    // Unlike other platforms, *BSD allows opening a directory as a file which
    // will cause all kinds of problems upstream (e.g., cpfile()). So we
    // detect and diagnose this. Note: not certain this is the case for NetBSD
    // and OpenBSD.
    //
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
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
    // Also note that _sopen() creates a dangling symlink target if _O_EXCL is
    // set. Thus, we need to prevent such a creation manually.
    //
    bool pass_perm (of & _O_CREAT);

    if (pass_perm)
    {
      if (file_exists (f)) // Follows symlink.
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
      else if (of & _O_EXCL)
      {
        pair<bool, entry_stat> pe (path_entry (f)); // Doesn't follow symlink.

        // Fail for a dangling symlink.
        //
        if (pe.first && pe.second.type == entry_type::symlink)
          throw_generic_ios_failure (EEXIST);
      }
    }

    // Make sure the file descriptor is not inheritable by default.
    //
    of |= _O_NOINHERIT;

    int fd;

    // For reasons unknown an attempt to open an existing file for writing
    // sometimes ends up with the EACCES POSIX error which is presumably a
    // translation of the ERROR_SHARING_VIOLATION system error returned by the
    // underlying CreateFile() function call (see mventry() for details). If
    // that's the case, we will keep trying to open the file for two seconds.
    //
    // Also, it turns out, if someone memory-maps a file, it takes Windows
    // some time to realize it's been unmapped and until then any attempt to
    // open it results in EINVAL POSIX error, ERROR_USER_MAPPED_FILE system
    // error. So we retry those as well.
    //
    for (size_t i (0); i < 41; ++i)
    {
      // Sleep 50 milliseconds before the open retry.
      //
      if (i != 0)
        Sleep (50);

      fd = pass_perm
           ? _sopen (f, of, _SH_DENYNO, pf)
           : _sopen (f, of, _SH_DENYNO);

      // Note that MSVCRT's _sopen() calls CreateFile() underneath,
      // translating the system error to POSIX on failure and bailing out
      // afterwords. Thus, we can query the original error code on _sopen()
      // failure.
      //
      // Note that MinGW's _sopen() is just a stub forwarding the call to the
      // (publicly available) MSVCRT's implementation.
      //
      if (!(fd == -1                             &&
            out                                  &&
            (errno == EACCES || errno == EINVAL) &&
            (GetLastError () == ERROR_SHARING_VIOLATION ||
             GetLastError () == ERROR_USER_MAPPED_FILE)))
        break;
    }

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

  uint64_t
  fdseek (int fd, int64_t o, fdseek_mode fdm)
  {
    int m (-1);

    switch (fdm)
    {
    case fdseek_mode::set: m = SEEK_SET; break;
    case fdseek_mode::cur: m = SEEK_CUR; break;
    case fdseek_mode::end: m = SEEK_END; break;
    }

#ifndef _WIN32
    off_t r (lseek (fd, static_cast<off_t> (o), m));
    if (r == static_cast<off_t> (-1))
      throw_generic_ios_failure (errno);
#else
    __int64 r (_lseeki64 (fd, o, m));
    if (r == -1)
      throw_generic_ios_failure (errno);
#endif

    return static_cast<uint64_t> (r);
  }

  // The rest is platform-specific.
  //
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
      auto_fd r (::dup (fd));
      if (r.get () == -1)
        throw_generic_ios_failure (errno);

      return r;
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
    auto_fd r (dup ());

    f = fcntl (r.get (), F_GETFD);
    if (f == -1 || fcntl (r.get (), F_SETFD, f | FD_CLOEXEC) == -1)
      throw_generic_ios_failure (errno);

    return r;
  }

  bool
  fdclose (int fd) noexcept
  {
    return close (fd) == 0;
  }

  auto_fd
  fdopen_null ()
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

    fdstream_mode r ((flags & O_NONBLOCK) == O_NONBLOCK
                     ? fdstream_mode::non_blocking
                     : fdstream_mode::blocking);

    if (flag (m, fdstream_mode::blocking) ||
        flag (m, fdstream_mode::non_blocking))
    {
      m &= fdstream_mode::blocking | fdstream_mode::non_blocking;

      // Should be exactly one blocking mode flag specified.
      //
      if (m != fdstream_mode::blocking && m != fdstream_mode::non_blocking)
        throw invalid_argument ("invalid blocking mode");

      // Set the new blocking mode if it differs from the current one.
      //
      if (m != r)
      {
        int new_flags (m == fdstream_mode::non_blocking
                       ? flags | O_NONBLOCK
                       : flags & ~O_NONBLOCK);

        if (fcntl (fd, F_SETFL, new_flags) == -1)
          throw_generic_ios_failure (errno);
      }
    }

    return r | fdstream_mode::binary;
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

  void
  fdtruncate (int fd, uint64_t n)
  {
    if (ftruncate (fd, static_cast<off_t> (n)) != 0)
      throw_generic_ios_failure (errno);
  }

  entry_stat
  fdstat (int fd)
  {
    struct stat s;
    if (fstat (fd, &s) != 0)
      throw_generic_error (errno);

    auto m (s.st_mode);
    entry_type t (entry_type::unknown);

    // Note: cannot be a symlink.
    //
    if (S_ISREG (m))
      t = entry_type::regular;
    else if (S_ISDIR (m))
      t = entry_type::directory;
    else if (S_ISBLK (m) || S_ISCHR (m) || S_ISFIFO (m) || S_ISSOCK (m))
      t = entry_type::other;

    return entry_stat {t, static_cast<uint64_t> (s.st_size)};
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

  bool
  fdterm_color (int, bool)
  {
    const char* t (std::getenv ("TERM"));

    // This test was lifted from GCC (Emacs shell sets TERM=dumb).
    //
    return t != nullptr && strcmp (t, "dumb") != 0;
  }

  static pair<size_t, size_t>
  fdselect (fdselect_set& read,
            fdselect_set& write,
            const chrono::milliseconds* timeout)
  {
    using namespace chrono;

    // Copy fdselect_set into the native fd_set, updating max_fd. Also clear
    // the ready flag in the source set.
    //
    int max_fd (-1);

    auto copy_set = [&max_fd] (fdselect_set& from, fd_set& to)
    {
      FD_ZERO (&to);

      for (fdselect_state& s: from)
      {
        s.ready = false;

        if (s.fd == nullfd)
          continue;

        if (s.fd < 0)
          throw invalid_argument ("invalid file descriptor");

        FD_SET (s.fd, &to);

        if (max_fd < s.fd)
          max_fd = s.fd;
      }
    };

    fd_set rds;
    fd_set wds;
    copy_set (read,  rds);
    copy_set (write, wds);

    if (max_fd == -1)
      throw invalid_argument ("empty file descriptor set");

    ++max_fd;

    // Note that if the timeout is not NULL, then the select timeout needs to
    // be recalculated for each select() call (of which we can potentially
    // have multiple due to EINTR). So the timeout can be used as bool.
    //
    timestamp now;
    timestamp deadline;

    if (timeout)
    {
      now = system_clock::now ();
      deadline = now + *timeout;
    }

    // Repeat the select() call while getting the EINTR error and throw on
    // any other error.
    //
    // Note that select() doesn't modify the sets on failure (according to
    // POSIX standard as well as to the Linux, FreeBSD and MacOS man pages).
    //
    for (timeval tv;;)
    {
      if (timeout)
      {
        if (now < deadline)
        {
          microseconds t (duration_cast<microseconds> (deadline - now));
          tv.tv_sec  = t.count () / 1000000;
          tv.tv_usec = t.count () % 1000000;
        }
        else
        {
          tv.tv_sec  = 0;
          tv.tv_usec = 0;
        }
      }

      int r (select (max_fd,
                     &rds,
                     &wds,
                     nullptr /* exceptfds */,
                     timeout ? &tv : nullptr));

      if (r == -1)
      {
        if (errno == EINTR)
        {
          if (timeout)
            now = system_clock::now ();

          continue;
        }

        throw_system_ios_failure (errno);
      }

      if (!timeout)
        assert (r != 0); // We don't expect the timeout to occur.

      break;
    }

    // Set the resulting ready states.
    //
    auto copy_states = [] (const fd_set& from, fdselect_set& to)
    {
      size_t r (0);
      for (fdselect_state& s: to)
      {
        if (s.fd == nullfd)
          continue;

        if (FD_ISSET (s.fd, &from))
        {
          ++r;
          s.ready = true;
        }
      }

      return r;
    };

    return make_pair (copy_states (rds, read), copy_states (wds, write));
  }

  streamsize
  fdread (int fd, void* buf, size_t n)
  {
    return read (fd, buf, n);
  }

  streamsize
  fdwrite (int fd, const void* buf, size_t n)
  {
    return write (fd, buf, n);
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
    auto dup = [fd] () -> auto_fd
    {
      auto_fd r (_dup (fd));
      if (r.get () == -1)
        throw_generic_ios_failure (errno);

      return r;
    };

    // We can not ammend file descriptors directly (nor obtain the flag value),
    // so need to resolve them to Windows HANDLE first. Note that we don't
    // need to close them (see fd_to_handle()).
    //
    DWORD f;
    if (!GetHandleInformation (fd_to_handle (fd), &f))
      throw_system_ios_failure (GetLastError ());

    // If the source handle is inheritable then no flag copy is required (as
    // the duplicate handle will be inheritable by default).
    //
    if (f & HANDLE_FLAG_INHERIT)
      return dup ();

    slock l (process_spawn_mutex);

    auto_fd r (dup ());
    if (!SetHandleInformation (fd_to_handle (r.get ()),
                               HANDLE_FLAG_INHERIT,
                               0 /* dwFlags */))
      throw_system_ios_failure (GetLastError ());

    return r;
  }

  bool
  fdclose (int fd) noexcept
  {
    return _close (fd) == 0;
  }

  auto_fd
  fdopen_null (bool temp)
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
      // Re-throw system_error as ios::failure, preserving the error category
      // and description.
      //
      int v (e.code ().value ());
      const error_category& c (e.code ().category ());

      if (c == generic_category ())
        throw_generic_ios_failure (v);
      else
      {
        assert (c == system_category ());
        throw_system_ios_failure (v, e.what ());
      }
    }
  }

  fdstream_mode
  fdmode (int fd, fdstream_mode m)
  {
    fdstream_mode r;

    // Get the current and set the new translation mode, if requested.
    //
    auto translation_mode = [fd] (fdstream_mode m)
    {
      assert (m == fdstream_mode::binary || m == fdstream_mode::text);

      // Note that _setmode() preserves the _O_NOINHERIT flag value.
      //
      int r (_setmode (fd, m == fdstream_mode::binary ? _O_BINARY : _O_TEXT));
      if (r == -1)
        throw_generic_ios_failure (errno); // EBADF or EINVAL (POSIX values).

      return (r & _O_BINARY) == _O_BINARY
             ? fdstream_mode::binary
             : fdstream_mode::text;
    };

    // It would have been natural not to change translation mode if none of
    // text or binary flags are passed. Unfortunately there is no (easy) way
    // to obtain the current mode for the file descriptor without setting a
    // new one. That's why we set the text mode (as the more common) to obtain
    // the current mode and revert it back if we didn't guess it right.
    //
    if (flag (m, fdstream_mode::text) || flag (m, fdstream_mode::binary))
    {
      fdstream_mode tm (m & (fdstream_mode::text | fdstream_mode::binary));

      // Should be exactly one translation flag specified.
      //
      if (tm != fdstream_mode::binary && tm != fdstream_mode::text)
        throw invalid_argument ("invalid translation mode");

      r = translation_mode (tm);
    }
    else
    {
      r = translation_mode (fdstream_mode::text);

      // Restore the mode if misguessed.
      //
      if (r == fdstream_mode::binary)
        translation_mode (r);
    }

    // Get the current and set the new blocking mode, if requested.
    //
    // Note that we always assume the blocking (synchronous) mode for file
    // descriptors other than pipes.
    //
    optional<handle_mode> pm (pipe_mode (fd));

    if (pm)
    {
      r |= pm->mode;

      if (flag (m, fdstream_mode::blocking) ||
          flag (m, fdstream_mode::non_blocking))
      {
        fdstream_mode bm (
          m & (fdstream_mode::blocking | fdstream_mode::non_blocking));

        // Should be exactly one blocking mode flag specified.
        //
        if (bm != fdstream_mode::blocking && bm != fdstream_mode::non_blocking)
          throw invalid_argument ("invalid blocking mode");

        // Set the new blocking mode if it differs from the current one.
        //
        if (bm != pm->mode)
        {
          DWORD flags (bm == fdstream_mode::non_blocking
                       ? PIPE_NOWAIT
                       : PIPE_WAIT);

          if (!SetNamedPipeHandleState (pm->handle,
                                        &flags,
                                        nullptr /* lpMaxCollectionCount */,
                                        nullptr /* lpCollectDataTimeout */))
            throw_system_ios_failure (GetLastError ());
        }
      }
    }
    else
    {
       r |= fdstream_mode::blocking;
    }

    return r;
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

  void
  fdtruncate (int fd, uint64_t n)
  {
    if (errno_t e = _chsize_s (fd, static_cast<__int64> (n)))
      throw_generic_ios_failure (e);
  }

  entry_stat
  fdstat (int fd)
  {
    // Since symlinks have been taken care of, we can just _fstat().
    //
    struct __stat64 s;
    if (_fstat64 (fd, &s) != 0)
      throw_generic_error (errno);

    auto m (s.st_mode);
    entry_type t (entry_type::unknown);

    if (S_ISREG (m))
      t = entry_type::regular;
    else if (S_ISDIR (m))
      t = entry_type::directory;
    else if (S_ISCHR (m))
      t = entry_type::other;

    return entry_stat {t, static_cast<uint64_t> (s.st_size)};
  }

  bool
  fdterm (int fd)
  {
    // @@ Both GCC and Clang simply call GetConsoleMode() for this check. I
    //    wonder why we don't do the same? See also fdterm_color() below.

    // We don't need to close it (see fd_to_handle()).
    //
    HANDLE h (fd_to_handle (fd));

    // Obtain the descriptor type.
    //
    DWORD t (GetFileType (h));
    DWORD e;

    if (t == FILE_TYPE_UNKNOWN && (e = GetLastError ()) != 0)
      throw_system_ios_failure (e);

    if (t == FILE_TYPE_CHAR) // Terminal.
    {
      // One notable special file that has this type is nul (as returned by
      // fdopen_null()). So tighten this case with the GetConsoleMode() call.
      //
      DWORD m;
      return GetConsoleMode (h, &m) != 0;
    }

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

    func f (function_cast<func> (
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

  bool
  fdterm_color (int fd, bool enable)
  {
    // We don't need to close it (see fd_to_handle()).
    //
    HANDLE h (fd_to_handle (fd));

    // See GH issue #312 for background on this logic.
    //
    DWORD m;
    if (!GetConsoleMode (h, &m))
      throw_system_ios_failure (GetLastError ());

    // Some terminals (e.g. Windows Terminal) enable VT processing by default.
    //
    if ((m & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0)
      return true;

    if (enable)
    {
      // If SetConsoleMode() fails, assume VT processing is unsupported (it
      // is only supported from a certain build of Windows 10).
      //
      // Note that Wine pretends to support this but doesn't handle the escape
      // sequences. See https://bugs.winehq.org/show_bug.cgi?id=49780.
      //
      if (SetConsoleMode (h,
                          (m                                 |
                           ENABLE_PROCESSED_OUTPUT           |
                           ENABLE_VIRTUAL_TERMINAL_PROCESSING)))
        return true;
    }

    return false;
  }

  static pair<size_t, size_t>
  fdselect (fdselect_set& read,
            fdselect_set& write,
            const chrono::milliseconds* timeout)
  {
    using namespace chrono;

    if (!write.empty ())
      throw invalid_argument ("write file descriptor set is not supported");

    // Validate and prepare the read set.
    //
    size_t n (0);

    for (fdselect_state& s: read)
    {
      s.ready = false;

      if (s.fd == nullfd)
        continue;

      if (s.fd < 0)
        throw invalid_argument ("invalid file descriptor");

      ++n;
    }

    if (n == 0)
      throw invalid_argument ("empty file descriptor set");

    // Note that if the timeout is not NULL, then the deadline needs to be
    // checked prior to re-probing the pipe for data presence. So the timeout
    // can be used as bool.
    //
    timestamp deadline;

    if (timeout)
      deadline = system_clock::now () + *timeout;

    // Keep iterating through the set until at least one byte can be read from
    // any of the pipes or any of them get closed (so can read eof).
    //
    size_t r (0);

    for (size_t i (0);; ++i)
    {
      for (fdselect_state& s: read)
      {
        if (s.fd == nullfd)
          continue;

        // Get the pipe handle. Note that PeekNamedPipe() doesn't care about
        // the blocking mode.
        //
        optional<handle_mode> m (pipe_mode (s.fd));

        if (!m)
          throw invalid_argument ("file descriptor is not a pipe");

        // Probe the pipe for data presence.
        //
        // Note that PeekNamedPipe() can block the thread execution in a
        // multi-threaded program. It is not clear from the documentation
        // under which circumstances and for how long this may happen. It
        // seems to relate to some internal IO synchronization mechanism and
        // probably depends on the contention level. But there doesn't appear
        // to be a better way. Specifically, WaitForMultipleObjects() does not
        // work on pipes (at least not on the non-overlapped ones).
        //
        char c;
        DWORD n;
        DWORD e (0);

        if (!PeekNamedPipe (m->handle,
                            &c,
                            1,
                            &n,
                            nullptr /* lpTotalBytesAvail */,
                            nullptr /* lpBytesLeftThisMessage */))
          e = GetLastError ();

        // If we successfully peeked a byte or failed due to the broken pipe
        // (the writing end is closed), then the descriptor is ready for
        // reading.
        //
        if ((e == 0 && n == 1) || e == ERROR_BROKEN_PIPE)
        {
          s.ready = true;
          ++r;
          continue;
        }

        // If we successfully read zero bytes, then there is no data available
        // for reading.
        //
        // Note that we never get the ERROR_NO_DATA error, but let's check for
        // good measure.
        //
        // As an observation, upon a successful call that peeks zero bytes,
        // GetLastError() returns zero (ERROR_SUCCESS) if no data were read
        // yet from the pipe and ERROR_NO_DATA otherwise.
        //
        if ((e == 0 && n == 0) || e == ERROR_NO_DATA)
          continue;

        // Both successful cases (n == 1 || n == 0) are already handled.
        //
        assert (e != 0);
        throw_system_ios_failure (e);
      }

      // Bail out if some descriptors are ready for reading or the deadline
      // has been reached, if specified, and sleep a bit and repeat otherwise.
      //
      if (r != 0)
        break;

      // Use exponential backoff but not too aggressive and with 25ms max.
      //
      DWORD t (
        static_cast<DWORD> (i <= 1000  ?  0 :
                            i >= 1000 + 100 ? 25 : 1 + ((i - 1000) / 4)));

      if (timeout)
      {
        timestamp now (system_clock::now ());

        if (now < deadline)
        {
          milliseconds tm (duration_cast<milliseconds> (deadline - now));

          if (t > tm.count ())
            t = static_cast<DWORD> (tm.count ());
        }
        else
          break;
      }

      if (t == 0)
        this_thread::yield ();
      else
        Sleep (t);
    }

    return make_pair (r, 0);
  }

  streamsize
  fdread (int fd, void* buf, size_t n)
  {
    // The _read() call fails with the EINVAL errno code either because
    // something is wrong with the arguments or there is no data to read from
    // a non-blocking pipe fd. In the latter case we translate the errno code
    // to EAGAIN.
    //
    // Note that this behavior is not really documented. However, it is both
    // described on the web and is confirmed by the tests. The ERROR_NO_DATA
    // system error is referred in the ReadFile() documentation in the context
    // of non-blocking pipes and is presumably set by the underlying
    // ReadFile() call.
    //
    // Also note that this doesn't work on Wine. We can probably implement the
    // non-blocking behavior for Wine using PeekNamedPipe() that is claimed to
    // also provide the number of total bytes available. More research is
    // required.

    // Clear last error to make sure ERROR_NO_DATA is set by _read().
    //
    SetLastError (0);

    int r (_read (fd, buf, static_cast<unsigned int> (n)));

    if (r == -1 && errno == EINVAL && GetLastError () == ERROR_NO_DATA)
    {
      // Let's also check that the file descriptor is really a non-blocking
      // pipe.
      //
      optional<handle_mode> pm (pipe_mode (fd, true /* ignore_error */));
      errno = pm && pm->mode == fdstream_mode::non_blocking ? EAGAIN : EINVAL;
    }

    return r;
  }

  streamsize
  fdwrite (int fd, const void* buf, size_t n)
  {
    return _write (fd, buf, static_cast<unsigned int> (n));
  }

#endif

  pair<size_t, size_t>
  fdselect (fdselect_set& read, fdselect_set& write)
  {
    return fdselect (read, write, nullptr /* timeout */);
  }

  template <>
  pair<size_t, size_t>
  fdselect (fdselect_set& read,
            fdselect_set& write,
            const chrono::milliseconds& timeout)
  {
    return fdselect (read, write, &timeout);
  }
}
