// file      : libbutl/fdstream.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <ios>     // streamsize
#include <vector>
#include <string>
#include <chrono>
#include <istream>
#include <ostream>
#include <utility> // move(), pair
#include <cstdint> // uint16_t, uint64_t
#include <cstddef> // size_t

#include <libbutl/path.hxx>
#include <libbutl/filesystem.hxx>   // permissions, entry_stat
#include <libbutl/small-vector.hxx>
#include <libbutl/bufstreambuf.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // RAII type for file descriptors. Note that failure to close the descriptor
  // is silently ignored by both the destructor and reset().
  //
  // The descriptor can be negative. Such a descriptor is treated as unopened
  // and is not closed.
  //
  struct nullfd_t
  {
    constexpr explicit nullfd_t (int) {}
    constexpr operator int () const {return -1;}
  };

  constexpr nullfd_t nullfd (-1);

  class LIBBUTL_SYMEXPORT auto_fd
  {
  public:
    auto_fd (nullfd_t = nullfd) noexcept: fd_ (-1) {}

    explicit
    auto_fd (int fd) noexcept: fd_ (fd) {}

    auto_fd (auto_fd&& fd) noexcept: fd_ (fd.release ()) {}
    auto_fd& operator= (auto_fd&&) noexcept;

    auto_fd (const auto_fd&) = delete;
    auto_fd& operator= (const auto_fd&) = delete;

    ~auto_fd () noexcept;

    int
    get () const noexcept {return fd_;}

    void
    reset (int fd = -1) noexcept;

    int
    release () noexcept
    {
      int r (fd_);
      fd_ = -1;
      return r;
    }

    // Close an open file descriptor. Throw ios::failure on the underlying OS
    // error. Reset the descriptor to -1 whether the exception is thrown or
    // not.
    //
    void
    close ();

  private:
    int fd_;
  };

  inline bool
  operator== (const auto_fd& x, const auto_fd& y)
  {
    return x.get () == y.get ();
  }

  inline bool
  operator!= (const auto_fd& x, const auto_fd& y)
  {
    return !(x == y);
  }

  inline bool
  operator== (const auto_fd& x, nullfd_t)
  {
    return x.get () == -1;
  }

  inline bool
  operator!= (const auto_fd& x, nullfd_t y)
  {
    return !(x == y);
  }

  // An [io]fstream that can be initialized with a file descriptor in addition
  // to a file name and that also by default enables exceptions on badbit and
  // failbit. So instead of a dance like this:
  //
  // ifstream ifs;
  // ifs.exceptions (ifstream::badbit | ifstream::failbit);
  // ifs.open (path.string ());
  //
  // You can simply do:
  //
  // ifdstream ifs (path);
  //
  // Notes and limitations:
  //
  // - char only
  // - input or output but not both (can use a union of two streams for that)
  // - no support for put back
  // - use of tell[gp]() and seek[gp]() is discouraged on Windows for
  //   fdstreams opened in the text mode (see fdstreambuf::seekoff()
  //   implementation for reasoning and consider using non-standard tellg()
  //   and seekg() in fdstreambuf, instead)
  // - non-blocking file descriptor is supported only by showmanyc() function
  //   and only for pipes on Windows, in contrast to POSIX systems
  // - throws ios::failure in case of open(), read(), write(), close(),
  //   seek[gp](), or tell[gp]() errors
  // - exception mask has at least badbit
  // - after catching an exception caused by badbit the stream is no longer
  //   usable
  // - not movable, though can be easily supported (or not: there is no move
  //   constructor for istream/ostream in GCC 4.9)
  // - passing to constructor auto_fd with a negative file descriptor is valid
  //   and results in the creation of an unopened object
  //
  class LIBBUTL_SYMEXPORT fdstreambuf: public bufstreambuf
  {
  public:
    // Reasonable (for stack allocation) buffer size that provides decent
    // performance.
    //
    static const std::size_t buffer_size = 8192;

    fdstreambuf () = default;

    // Unless specified, the current read/write position is assumed to
    // be 0 (note: not queried).
    //
    fdstreambuf (auto_fd&&, std::uint64_t pos = 0);

    // Before we invented auto_fd into fdstreams we keept fdstreambuf opened
    // on faulty close attempt. Now fdstreambuf is always closed by close()
    // function.  This semantics change seems to be the right one as there is
    // no reason to expect fdclose() to succeed after it has already failed
    // once.
    //
    void
    close () {fd_.close ();}

    auto_fd
    release ();

    void
    open (auto_fd&&, std::uint64_t pos = 0);

    bool
    is_open () const {return fd_.get () >= 0;}

    int
    fd () const {return fd_.get ();}

    // Set the file descriptor blocking mode returning the previous mode on
    // success and throwing ios::failure otherwise (see fdmode() for details).
    //
    // Note that besides calling fdmode(fd()), this function also updating its
    // internal state according to the new mode.
    //
    bool
    blocking (bool);

    bool
    blocking () const {return !non_blocking_;}

  public:
    using base = bufstreambuf;

    // basic_streambuf input interface.
    //
  public:
    virtual std::streamsize
    showmanyc ();

    virtual int_type
    underflow ();

    // Direct access to the get area. Use with caution.
    //
    using base::gptr;
    using base::egptr;
    using base::gbump;

    // Return the (logical) position of the next byte to be read.
    //
    using base::tellg;

    // Seek to the (logical) position as if by reading the specified number of
    // bytes from the beginning of the stream. Throw ios::failure on the
    // underlying OS errors.
    //
    void
    seekg (std::uint64_t);

  private:
    bool
    load ();

    // basic_streambuf output interface.
    //
  public:
    virtual int_type
    overflow (int_type);

    virtual int
    sync ();

    virtual std::streamsize
    xsputn (const char_type*, std::streamsize);

    // Return the (logical) position of the next byte to be written.
    //
    using base::tellp;

    // basic_streambuf positioning interface (both input/output).
    //
  public:
    virtual pos_type
    seekpos (pos_type, std::ios_base::openmode);

    virtual pos_type
    seekoff (off_type, std::ios_base::seekdir, std::ios_base::openmode);

  private:
    bool
    save ();

  private:
    auto_fd fd_;
    char buf_[buffer_size];
    bool non_blocking_ = false;
  };

  // File stream mode.
  //
  // The text/binary flags have the same semantics as those in std::fstream.
  // Specifically, this is a noop for POSIX systems where the two modes are
  // the same. On Windows, when reading in the text mode the sequence of 0xD,
  // 0xA characters is translated into the single OxA character and 0x1A is
  // interpreted as EOF. When writing in the text mode the OxA character is
  // translated into the 0xD, 0xA sequence.
  //
  // The skip flag instructs the stream to skip to the end before closing the
  // file descriptor. This is primarily useful when working with pipes where
  // you may want not to "offend" the other end by closing your end before
  // reading all the data.
  //
  // The blocking/non_blocking flags determine whether the IO operation should
  // block or return control if currently there is no data to read or no room
  // to write. Only the istream::readsome() function supports the semantics of
  // non-blocking operations. In contrast to POSIX systems, we only support
  // this for pipes on Windows, always assuming the blocking mode for other
  // file descriptors. IO stream operations other than readsome() are illegal
  // in the non-blocking mode and result in the badbit being set (note that
  // it is not the more appropriate failbit for implementation reasons).
  //
  enum class fdstream_mode: std::uint16_t
  {
    text         = 0x01,
    binary       = 0x02,
    skip         = 0x04,
    blocking     = 0x08,
    non_blocking = 0x10,

    none = 0
  };

  inline fdstream_mode operator& (fdstream_mode, fdstream_mode);
  inline fdstream_mode operator| (fdstream_mode, fdstream_mode);
  inline fdstream_mode operator&= (fdstream_mode&, fdstream_mode);
  inline fdstream_mode operator|= (fdstream_mode&, fdstream_mode);

  // Extended (compared to ios::openmode) file open flags.
  //
  enum class fdopen_mode: std::uint16_t
  {
    in         = 0x01, // Open for reading.
    out        = 0x02, // Open for writing.
    append     = 0x04, // Seek to the end of file before each write.
    truncate   = 0x08, // Discard the file contents on open.
    create     = 0x10, // Create a file if not exists.
    exclusive  = 0x20, // Fail if the file exists and the create flag is set.
    binary     = 0x40, // Set binary translation mode.
    at_end     = 0x80, // Seek to the end of stream immediately after open.

    none = 0           // Usefull when building the mode incrementally.
  };

  inline fdopen_mode operator& (fdopen_mode, fdopen_mode);
  inline fdopen_mode operator| (fdopen_mode, fdopen_mode);
  inline fdopen_mode operator&= (fdopen_mode&, fdopen_mode);
  inline fdopen_mode operator|= (fdopen_mode&, fdopen_mode);

  class LIBBUTL_SYMEXPORT fdstream_base
  {
  protected:
    fdstream_base () = default;
    fdstream_base (auto_fd&&, std::uint64_t pos);
    fdstream_base (auto_fd&&, fdstream_mode, std::uint64_t pos);

  public:
    int
    fd () const {return buf_.fd ();}

    bool
    blocking () const {return buf_.blocking ();}

  protected:
    fdstreambuf buf_;
  };

  // iofdstream constructors and open() functions that take openmode as an
  // argument mimic the corresponding iofstream functions in terms of the
  // openmode mask interpretation. They throw std::invalid_argument for an
  // invalid combination of flags (as per the standard). Note that the in and
  // out flags are always added implicitly for ifdstream and ofdstream,
  // respectively.
  //
  // iofdstream constructors and open() functions that take fdopen_mode as an
  // argument interpret the mask literally just ignoring some flags which are
  // meaningless in the absense of others (read more on that in the comment
  // for fdopen()). Note that the in and out flags are always added implicitly
  // for ifdstream and ofdstream, respectively.
  //
  // iofdstream constructors and open() functions that take file path as a
  // const std::string& or const char* may throw the invalid_path exception.
  //
  // Passing auto_fd with a negative file descriptor is valid and results in
  // the creation of an unopened object.
  //
  // Also note that open() and close() functions can be successfully called
  // for an opened and unopened objects respectively. That is in contrast with
  // iofstream that sets failbit in such cases.
  //

  // Note that ifdstream destructor will close an open file descriptor but
  // will ignore any errors. To detect such errors, call close() explicitly.
  //
  // This is a sample usage of iofdstreams with process. Note that here it is
  // expected that the child process reads from STDIN first and writes to
  // STDOUT afterwards.
  //
  // try
  // {
  //   process pr (args, -1, -1);
  //
  //   try
  //   {
  //     // In case of exception, skip and close input after output.
  //     //
  //     ifdstream is (move (pr.in_ofd), fdstream_mode::skip);
  //     ofdstream os (move (pr.out_fd));
  //
  //     // Write.
  //
  //     os.close (); // Don't block the other end.
  //
  //     // Read.
  //
  //     is.close (); // Skip till end and close.
  //
  //     if (pr.wait ())
  //     {
  //       return ...; // Good.
  //     }
  //
  //     // Non-zero exit, diagnostics presumably issued, fall through.
  //   }
  //   catch (const failure&)
  //   {
  //     // IO failure, child exit status doesn't matter. Just wait for the
  //     // process completion and fall through.
  //     //
  //     // Note that this is optional if the process_error handler simply
  //     // falls through since process destructor will wait (but will ignore
  //     // any errors).
  //     //
  //     pr.wait ();
  //   }
  //
  //   error <<  .... ;
  //
  //   // Fall through.
  // }
  // catch (const process_error& e)
  // {
  //   error << ... << e;
  //
  //   if (e.child ())
  //     exit (1);
  //
  //   // Fall through.
  // }
  //
  // throw failed ();
  //
  class LIBBUTL_SYMEXPORT ifdstream: public fdstream_base, public std::istream
  {
  public:
    // Create an unopened object.
    //
    explicit
    ifdstream (iostate = badbit | failbit);

    explicit
    ifdstream (auto_fd&&,
               iostate = badbit | failbit,
               std::uint64_t pos = 0);

    ifdstream (auto_fd&&,
               fdstream_mode m,
               iostate = badbit | failbit,
               std::uint64_t pos = 0);

    explicit
    ifdstream (const char*,
               iostate = badbit | failbit);

    explicit
    ifdstream (const std::string&,
               iostate = badbit | failbit);

    explicit
    ifdstream (const path&,
               iostate = badbit | failbit);

    // @@ In some implementations (for example, MSVC), iostate and openmode
    //    (and/or their respective constants) are not distinct enough which
    //    causes overload resolution errors.
    //
    ifdstream (const char*,
               openmode,
               iostate /*= badbit | failbit*/);

    ifdstream (const std::string&,
               openmode,
               iostate /*= badbit | failbit*/);

    ifdstream (const path&,
               openmode,
               iostate /*= badbit | failbit*/);

    ifdstream (const char*,
               fdopen_mode,
               iostate = badbit | failbit);

    ifdstream (const std::string&,
               fdopen_mode,
               iostate = badbit | failbit);

    ifdstream (const path&,
               fdopen_mode,
               iostate = badbit | failbit);

    ~ifdstream () override;

    void
    open (const char*, openmode = in);

    void
    open (const std::string&, openmode = in);

    void
    open (const path&, openmode = in);

    void
    open (const char*, fdopen_mode);

    void
    open (const std::string&, fdopen_mode);

    void
    open (const path&, fdopen_mode);

    void
    open (auto_fd&& fd, std::uint64_t pos = 0)
    {
      buf_.open (std::move (fd), pos);
      clear ();
    }

    void
    open (auto_fd&& fd, fdstream_mode m, std::uint64_t pos = 0);

    void close ();
    auto_fd release (); // Note: no skipping.
    bool is_open () const {return buf_.is_open ();}

    // Read the textual stream. The stream is supposed not to contain the null
    // character.
    //
    std::string
    read_text ();

    // Read the binary stream.
    //
    std::vector<char>
    read_binary ();

  private:
    bool skip_ = false;
  };

  // Note that ofdstream requires that you explicitly call close() before
  // destroying it. Or, more specifically, the ofdstream object should not be
  // in the opened state by the time its destructor is called, unless it is in
  // the "not good" state (good() == false) or the destructor is being called
  // during the stack unwinding due to an exception being thrown
  // (std::uncaught_exception() == true). This is enforced with assert() in
  // the ofdstream destructor.
  //
  class LIBBUTL_SYMEXPORT ofdstream: public fdstream_base, public std::ostream
  {
  public:
    // Create an unopened object.
    //
    explicit
    ofdstream (iostate = badbit | failbit);

    explicit
    ofdstream (auto_fd&&,
               iostate = badbit | failbit,
               std::uint64_t pos = 0);

    ofdstream (auto_fd&&,
               fdstream_mode m,
               iostate = badbit | failbit,
               std::uint64_t pos = 0);

    explicit
    ofdstream (const char*,
               iostate = badbit | failbit);

    explicit
    ofdstream (const std::string&,
               iostate = badbit | failbit);

    explicit
    ofdstream (const path&,
               iostate = badbit | failbit);

    // @@ In some implementations (for example, MSVC), iostate and openmode
    //    (and/or their respective constants) are not distinct enough which
    //    causes overload resolution errors.
    //
    ofdstream (const char*,
               openmode,
               iostate /*= badbit | failbit*/);

    ofdstream (const std::string&,
               openmode,
               iostate /*= badbit | failbit*/);

    ofdstream (const path&,
               openmode,
               iostate /*= badbit | failbit*/);

    ofdstream (const char*,
               fdopen_mode,
               iostate = badbit | failbit);

    ofdstream (const std::string&,
               fdopen_mode,
               iostate = badbit | failbit);

    ofdstream (const path&,
               fdopen_mode,
               iostate = badbit | failbit);

    ~ofdstream () override;

    void
    open (const char*, openmode = out);

    void
    open (const std::string&, openmode = out);

    void
    open (const path&, openmode = out);

    void
    open (const char*, fdopen_mode);

    void
    open (const std::string&, fdopen_mode);

    void
    open (const path&, fdopen_mode);

    void
    open (auto_fd&& fd, std::uint64_t pos = 0)
    {
      buf_.open (std::move (fd), pos);
      clear ();
    }

    void close () {if (is_open ()) flush (); buf_.close ();}
    auto_fd release ();
    bool is_open () const {return buf_.is_open ();}
  };

  // Open a file or, if the file name is `-`, stdin/stdout.
  //
  // In case of the stdin/stdout, these functions simply adjust the exception
  // mask on std::cin/cout to match the i/ofdstreams argument.
  //
  // Return a reference to the opened i/ofdstream or cin/cout and, in the
  // latter case, set the translated name in path_name to <stdin>/<stdout>
  // (unless it is already present).
  //
  // Note that ofdstream::close() should be called explicitly unless stdout
  // was opened (but harmless to call even if it was).
  //
  LIBBUTL_SYMEXPORT std::istream&
  open_file_or_stdin (path_name&, ifdstream&);

  LIBBUTL_SYMEXPORT std::ostream&
  open_file_or_stdout (path_name&, ofdstream&);

  // The std::getline() replacement that provides a workaround for libstdc++'s
  // ios::failure ABI fiasco (#66145) by throwing ios::failure, as it is
  // defined at libbutl build time (new ABI on recent distributions) rather
  // than libstdc++ build time (still old ABI on most distributions).
  //
  // Notes:
  //
  // - This relies of ADL so if the stream is used via the std::istream
  //   interface, then std::getline() will still be used. To put it another
  //   way, this is "the best we can do" until GCC folks get their act
  //   together.
  //
  // - The fail and eof bits may be left cleared in the stream exception mask
  //   when the function throws because of badbit.
  //
  LIBBUTL_SYMEXPORT ifdstream&
  getline (ifdstream&, std::string&, char delim = '\n');

  // The non-blocking getline() version that reads the line in potentially
  // multiple calls. Key differences compared to getline():
  //
  // - Stream must be in the non-blocking mode and exception mask must have
  //   at least badbit.
  //
  // - Return type is bool instead of stream. Return true if the line has been
  //   read or false if it should be called again once the stream has more
  //   data to read. Also return true on failure.
  //
  // - The string must be empty on the first call.
  //
  // - There could still be data to read in the stream's buffer (as opposed to
  //   file descriptor) after this function returns true and you should be
  //   careful not to block on fdselect() in this case. In fact, the
  //   recommended pattern is to call this function first and only call
  //   fdselect() if it returns false.
  //
  // The typical usage in combination with the eof() helper:
  //
  // fdselect_set fds {is.fd (), ...};
  // fdselect_state& ist (fds[0]);
  // fdselect_state& ...;
  //
  // for (string l; ist.fd != nullfd || ...; )
  // {
  //   if (ist.fd != nullfd && getline_non_blocking (is, l))
  //   {
  //     if (eof (is))
  //       ist.fd = nullfd;
  //     else
  //     {
  //       // Consume line.
  //
  //       l.clear ();
  //     }
  //
  //     continue;
  //   }
  //
  //   ifdselect (fds);
  //
  //   // Handle other ready fds.
  // }
  //
  LIBBUTL_SYMEXPORT bool
  getline_non_blocking (ifdstream&, std::string&, char delim = '\n');

  // Open a file returning an auto_fd that holds its file descriptor on
  // success and throwing ios::failure otherwise.
  //
  // The mode argument should have at least one of the in or out flags set.
  // The append and truncate flags are meaningless in the absense of the out
  // flag and are ignored without it. The exclusive flag is meaningless in the
  // absense of the create flag and is ignored without it. Note also that if
  // the exclusive flag is specified then a dangling symbolic link is treated
  // as an existing file.
  //
  // The permissions argument is taken into account only if the file is
  // created. Note also that permissions can be adjusted while being set in a
  // way specific for the OS. On POSIX systems they are modified with the
  // process' umask, so effective permissions are permissions & ~umask. On
  // Windows permissions other than ru and wu are unlikelly to have effect.
  //
  // Also note that on POSIX the FD_CLOEXEC flag is set for the file descriptor
  // to prevent its leakage into child processes. On Windows, for the same
  // purpose, the _O_NOINHERIT flag is set. Note that the process class, that
  // passes such a descriptor to the child, makes it inheritable for a while.
  //
  LIBBUTL_SYMEXPORT auto_fd
  fdopen (const char*,
          fdopen_mode,
          permissions = permissions::ru | permissions::wu |
                        permissions::rg | permissions::wg |
                        permissions::ro | permissions::wo);

  LIBBUTL_SYMEXPORT auto_fd
  fdopen (const std::string&,
          fdopen_mode,
          permissions = permissions::ru | permissions::wu |
                        permissions::rg | permissions::wg |
                        permissions::ro | permissions::wo);

  LIBBUTL_SYMEXPORT auto_fd
  fdopen (const path&,
          fdopen_mode,
          permissions = permissions::ru | permissions::wu |
                        permissions::rg | permissions::wg |
                        permissions::ro | permissions::wo);

  // Duplicate an open file descriptor. Throw ios::failure on the underlying
  // OS error.
  //
  // Note that on POSIX the FD_CLOEXEC flag is set for the new descriptor if it
  // is present for the source one. That's in contrast to POSIX dup() that
  // doesn't copy file descriptor flags. Also note that duplicating descriptor
  // and setting the flag is not an atomic operation generally, but it is in
  // regards to child process spawning (to prevent file descriptor leakage into
  // a child process).
  //
  // Note that on Windows the _O_NOINHERIT flag is set for the new descriptor
  // if it is present for the source one. That's in contrast to Windows _dup()
  // that doesn't copy the flag. Also note that duplicating descriptor and
  // setting the flag is not an atomic operation generally, but it is in
  // regards to child process spawning (to prevent file descriptor leakage into
  // a child process).
  //
  LIBBUTL_SYMEXPORT auto_fd
  fddup (int fd);

  // Set the translation and/or blocking modes for the file descriptor. Throw
  // invalid_argument for an invalid combination of flags. Return the previous
  // mode on success, throw ios::failure otherwise.
  //
  // The text and binary flags are mutually exclusive on Windows. On POSIX
  // system the two modes are the same and so no check is performed.
  //
  // The blocking and non-blocking flags are mutually exclusive. In contrast
  // to POSIX systems, on Windows the non-blocking mode is only supported for
  // pipes, with the blocking mode assumed for other file descriptors
  // regardless of the flags.
  //
  // Note that on Wine currently pipes always behave as blocking regardless of
  // the mode set.
  //
  LIBBUTL_SYMEXPORT fdstream_mode
  fdmode (int, fdstream_mode);

  // Portable functions for obtaining file descriptors of standard streams.
  // Throw ios::failure on the underlying OS error.
  //
  // Note that you normally wouldn't want to close them using fddup() to
  // convert them to auto_fd, for example:
  //
  // ifdstream is (fddup (stdin_fd ()));
  //
  LIBBUTL_SYMEXPORT int stdin_fd  ();
  LIBBUTL_SYMEXPORT int stdout_fd ();
  LIBBUTL_SYMEXPORT int stderr_fd ();

  // Convenience functions for setting the translation mode for standard
  // streams.
  //
  LIBBUTL_SYMEXPORT fdstream_mode stdin_fdmode  (fdstream_mode);
  LIBBUTL_SYMEXPORT fdstream_mode stdout_fdmode (fdstream_mode);
  LIBBUTL_SYMEXPORT fdstream_mode stderr_fdmode (fdstream_mode);

  // Low-level, nothrow file descriptor API.
  //

  // Close the file descriptor. Return true on success, set errno and return
  // false otherwise.
  //
  LIBBUTL_SYMEXPORT bool
  fdclose (int) noexcept;

  // Open the null device (e.g., /dev/null) that discards all data written to
  // it and provides no data for read operations (i.e., yelds EOF on read).
  // Return an auto_fd that holds its file descriptor on success and throwing
  // ios::failure otherwise.
  //
  // On Windows the null device is NUL and writing anything substantial to it
  // (like redirecting a process' output) is extremely slow, as in, an order
  // of magnitude slower than writing to disk. If you are using the descriptor
  // yourself this can be mitigated by setting the binary mode (already done
  // by fdopen()) and using a buffer of around 64K. However, sometimes you
  // have no control of how the descriptor will be used. For instance, it can
  // be used to redirect a child's stdout and the way the child sets up its
  // stdout is out of your control (on Windows). For such cases, there is an
  // emulation via a temporary file. Mostly it functions as a proper null
  // device with the file automatically removed once the descriptor is
  // closed. One difference, however, would be if you were to both write to
  // and read from the descriptor.
  //
  // Note that on POSIX the FD_CLOEXEC flag is set for the file descriptor to
  // prevent its leakage into child processes. On Windows, for the same
  // purpose, the _O_NOINHERIT flag is set.
  //
#ifndef _WIN32
  LIBBUTL_SYMEXPORT auto_fd
  fdopen_null ();
#else
  LIBBUTL_SYMEXPORT auto_fd
  fdopen_null (bool temp = false);
#endif

  struct fdpipe
  {
    auto_fd in;
    auto_fd out;

    void
    close ()
    {
      in.close ();
      out.close ();
    }
  };

  // Create a pipe. Throw ios::failure on the underlying OS error. By default
  // both ends of the pipe are opened in the text mode. Pass the binary flag
  // to instead open them in the binary mode. Passing a mode other than none
  // or binary is illegal.
  //
  // Note that on Windows both ends of the created pipe are not inheritable.
  // In particular, the process class that uses fdpipe underneath makes the
  // appropriate end (the one being passed to the child) inheritable.
  //
  // Note that on POSIX the FD_CLOEXEC flag is set for both ends, so they get
  // automatically closed by the child process to prevent undesired behaviors
  // (such as child deadlock on read from a pipe due to the write-end leakage
  // into the child process). Opening a pipe and setting the flag is not an
  // atomic operation generally, but it is in regards to child process spawning
  // (to prevent file descriptor leakage into child processes spawned from
  // other threads). Also note that you don't need to reset the flag for a pipe
  // end being passed to the process class ctor.
  //
  LIBBUTL_SYMEXPORT fdpipe
  fdopen_pipe (fdopen_mode = fdopen_mode::none);

  // Seeking.
  //
  enum class fdseek_mode {set, cur, end};

  LIBBUTL_SYMEXPORT std::uint64_t
  fdseek (int, std::int64_t, fdseek_mode);

  // Truncate or expand the file to the specified size. Throw ios::failure on
  // the underlying OS error.
  //
  LIBBUTL_SYMEXPORT void
  fdtruncate (int, std::uint64_t);

  // Return filesystem entry stat from file descriptor. Throw ios::failure on
  // the underlying OS error.
  //
  // See also path_entry() in filesystem.
  //
  LIBBUTL_SYMEXPORT entry_stat
  fdstat (int);

  // Test whether a file descriptor refers to a terminal. Throw ios::failure
  // on the underlying OS error.
  //
  LIBBUTL_SYMEXPORT bool
  fdterm (int);

  // Test whether a terminal file descriptor supports ANSI color output. If
  // the enable argument is true, then also try to enable color output (only
  // applicable on some platforms, such as Windows). Throw ios::failure on the
  // underlying OS error.
  //
  LIBBUTL_SYMEXPORT bool
  fdterm_color (int, bool enable);

  // Wait until one or more file descriptors becomes ready for input (reading)
  // or output (writing). Return the pair of numbers of descriptors that are
  // ready. Throw std::invalid_argument if anything is wrong with arguments
  // (both sets are empty, invalid fd, etc). Throw ios::failure on the
  // underlying OS error.
  //
  // Note that the function clears all the previously-ready entries on each
  // call. Entries with nullfd are ignored (but cleared).
  //
  // On Windows only pipes and only their input (read) ends are supported.
  //
  struct fdselect_state
  {
    int  fd;
    bool ready;
    void* data; // Arbitrary data which can be associated with the descriptor.

    // Note: intentionally non-explicit to allow implicit initialization when
    // pushing to fdselect_set.
    //
    fdselect_state (int fd, void* d = nullptr)
        : fd (fd), ready (false), data (d) {}
  };

  using fdselect_set = small_vector<fdselect_state, 4>;

  LIBBUTL_SYMEXPORT std::pair<std::size_t, std::size_t>
  fdselect (fdselect_set& ifds, fdselect_set& ofds);

  inline std::size_t
  ifdselect (fdselect_set& ifds)
  {
    fdselect_set ofds;
    return fdselect (ifds, ofds).first;
  }

  inline std::size_t
  ofdselect (fdselect_set& ofds)
  {
    fdselect_set ifds;
    return fdselect (ifds, ofds).second;
  }

  // As above but wait up to the specified timeout returning a pair of zeroes
  // if none of the descriptors became ready.
  //
  template <typename R, typename P>
  std::pair<std::size_t, std::size_t>
  fdselect (fdselect_set&, fdselect_set&, const std::chrono::duration<R, P>&);

  template <typename R, typename P>
  inline std::size_t
  ifdselect (fdselect_set& ifds, const std::chrono::duration<R, P>& timeout)
  {
    fdselect_set ofds;
    return fdselect (ifds, ofds, timeout).first;
  }

  template <typename R, typename P>
  inline std::size_t
  ofdselect (fdselect_set& ofds, const std::chrono::duration<R, P>& timeout)
  {
    fdselect_set ifds;
    return fdselect (ifds, ofds, timeout).second;
  }

  // POSIX read() function wrapper. In particular, it supports the semantics
  // of non-blocking read for pipes on Windows.
  //
  // Note that on Wine currently pipes always behave as blocking regardless of
  // the mode.
  //
  LIBBUTL_SYMEXPORT std::streamsize
  fdread (int, void*, std::size_t);

  // POSIX write() function wrapper, for uniformity.
  //
  LIBBUTL_SYMEXPORT std::streamsize
  fdwrite (int, const void*, std::size_t);
}

#include <libbutl/fdstream.ixx>
