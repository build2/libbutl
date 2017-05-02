// file      : libbutl/fdstream.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_FDSTREAM_HXX
#define LIBBUTL_FDSTREAM_HXX

#include <string>
#include <istream>
#include <ostream>
#include <utility> // move()
#include <cstdint> // uint16_t

#include <libbutl/export.hxx>

#include <libbutl/path.hxx>
#include <libbutl/filesystem.hxx> // permissions

namespace butl
{
  // RAII type for file descriptors. Note that failure to close the descriptor
  // is silently ignored by both the destructor and reset().
  //
  // The descriptor can be negative. Such a descriptor is treated as unopened
  // and is not closed.
  //
  struct nullfd_t {constexpr explicit nullfd_t (int) {}};
  constexpr const nullfd_t nullfd (-1);

  class LIBBUTL_EXPORT auto_fd
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
  // - input or output but not both
  // - no support for put back
  // - non-blocking file descriptor is supported only by showmanyc() function
  //   and only on POSIX
  // - throws ios::failure in case of open()/read()/write()/close() errors
  // - exception mask has at least badbit
  // - after catching an exception caused by badbit the stream is no longer
  //   used
  // - not movable, though can be easily supported
  // - passing to constructor auto_fd with a negative file descriptor is valid
  //   and results in the creation of an unopened object
  //
  class LIBBUTL_EXPORT fdbuf: public std::basic_streambuf<char>
  {
  public:
    fdbuf () = default;
    fdbuf (auto_fd&&);

    // Before we invented auto_fd into fdstreams we keept fdbuf opened on
    // faulty close attempt. Now fdbuf is always closed by close() function.
    // This semantics change seems to be the right one as there is no reason to
    // expect fdclose() to succeed after it has already failed once.
    //
    void
    close () {fd_.close ();}

    auto_fd
    release ();

    void
    open (auto_fd&&);

    bool
    is_open () const {return fd_.get () >= 0;}

    int
    fd () const {return fd_.get ();}

  public:
    using int_type = std::basic_streambuf<char>::int_type;
    using traits_type = std::basic_streambuf<char>::traits_type;

    // basic_streambuf input interface.
    //
  public:
    virtual std::streamsize
    showmanyc ();

    virtual int_type
    underflow ();

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

  private:
    bool
    save ();

  private:
    auto_fd fd_;
    char buf_[8192];
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
  // non-blocking operations. We also only support this on POSIX (Windows does
  // not provide means for the non-blocking reading from a file descriptor so
  // these flags are noop there). IO stream operations other than readsome()
  // are illegal for non_blocking mode and result in the badbit being set.
  //
  enum class fdstream_mode: std::uint16_t
  {
    text         = 0x01,
    binary       = 0x02,
    skip         = 0x04,
    blocking     = 0x08,
    non_blocking = 0x10
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

    none = 0           // Usefull when build the mode incrementally.
  };

  inline fdopen_mode operator& (fdopen_mode, fdopen_mode);
  inline fdopen_mode operator| (fdopen_mode, fdopen_mode);
  inline fdopen_mode operator&= (fdopen_mode&, fdopen_mode);
  inline fdopen_mode operator|= (fdopen_mode&, fdopen_mode);

  class LIBBUTL_EXPORT fdstream_base
  {
  protected:
    fdstream_base () = default;
    fdstream_base (auto_fd&& fd): buf_ (std::move (fd)) {}
    fdstream_base (auto_fd&&, fdstream_mode);

  public:
    int
    fd () const {return buf_.fd ();}

  protected:
    fdbuf buf_;
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
  class LIBBUTL_EXPORT ifdstream: public fdstream_base, public std::istream
  {
  public:
    // Create an unopened object.
    //
    explicit
    ifdstream (iostate e = badbit | failbit);

    explicit
    ifdstream (auto_fd&&, iostate e = badbit | failbit);
    ifdstream (auto_fd&&, fdstream_mode m, iostate e = badbit | failbit);

    explicit
    ifdstream (const char*,
               openmode = in,
               iostate e = badbit | failbit);

    explicit
    ifdstream (const std::string&,
               openmode = in,
               iostate e = badbit | failbit);

    explicit
    ifdstream (const path&,
               openmode = in,
               iostate e = badbit | failbit);

    ifdstream (const char*,
               fdopen_mode,
               iostate e = badbit | failbit);

    ifdstream (const std::string&,
               fdopen_mode,
               iostate e = badbit | failbit);

    ifdstream (const path&,
               fdopen_mode,
               iostate e = badbit | failbit);

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
    open (auto_fd&& fd) {buf_.open (std::move (fd)); clear ();}

    void close ();
    auto_fd release (); // Note: no skipping.
    bool is_open () const {return buf_.is_open ();}

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
  class LIBBUTL_EXPORT ofdstream: public fdstream_base, public std::ostream
  {
  public:
    // Create an unopened object.
    //
    explicit
    ofdstream (iostate e = badbit | failbit);

    explicit
    ofdstream (auto_fd&&, iostate e = badbit | failbit);
    ofdstream (auto_fd&&, fdstream_mode m, iostate e = badbit | failbit);

    explicit
    ofdstream (const char*,
               openmode = out,
               iostate e = badbit | failbit);

    explicit
    ofdstream (const std::string&,
               openmode = out,
               iostate e = badbit | failbit);

    explicit
    ofdstream (const path&,
               openmode = out,
               iostate e = badbit | failbit);

    ofdstream (const char*,
               fdopen_mode,
               iostate e = badbit | failbit);

    ofdstream (const std::string&,
               fdopen_mode,
               iostate e = badbit | failbit);

    ofdstream (const path&,
               fdopen_mode,
               iostate e = badbit | failbit);

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
    open (auto_fd&& fd) {buf_.open (std::move (fd)); clear ();}

    void close () {if (is_open ()) flush (); buf_.close ();}
    auto_fd release ();
    bool is_open () const {return buf_.is_open ();}
  };

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
  LIBBUTL_EXPORT ifdstream&
  getline (ifdstream&, std::string&, char delim = '\n');

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
  LIBBUTL_EXPORT auto_fd
  fdopen (const char*,
          fdopen_mode,
          permissions = permissions::ru | permissions::wu |
                        permissions::rg | permissions::wg |
                        permissions::ro | permissions::wo);

  LIBBUTL_EXPORT auto_fd
  fdopen (const std::string&,
          fdopen_mode,
          permissions = permissions::ru | permissions::wu |
                        permissions::rg | permissions::wg |
                        permissions::ro | permissions::wo);

  LIBBUTL_EXPORT auto_fd
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
  LIBBUTL_EXPORT auto_fd
  fddup (int fd);

  // Set the translation mode for the file descriptor. Throw invalid_argument
  // for an invalid combination of flags. Return the previous mode on success,
  // throw ios::failure otherwise.
  //
  // The text and binary flags are mutually exclusive on Windows. Due to
  // implementation details at least one of them should be specified. On POSIX
  // system the two modes are the same and so no check is performed.
  //
  // The blocking and non-blocking flags are mutually exclusive on POSIX
  // system. Non-blocking mode is not supported on Windows and so the blocking
  // mode is assumed regardless of the flags.
  //
  LIBBUTL_EXPORT fdstream_mode
  fdmode (int, fdstream_mode);

  // Convenience functions for setting the translation mode for standard
  // streams.
  //
  LIBBUTL_EXPORT fdstream_mode stdin_fdmode  (fdstream_mode);
  LIBBUTL_EXPORT fdstream_mode stdout_fdmode (fdstream_mode);
  LIBBUTL_EXPORT fdstream_mode stderr_fdmode (fdstream_mode);

  // Low-level, nothrow file descriptor API.
  //

  // Close the file descriptor. Return true on success, set errno and return
  // false otherwise.
  //
  LIBBUTL_EXPORT bool
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
  LIBBUTL_EXPORT auto_fd
  fdnull () noexcept;
#else
  LIBBUTL_EXPORT auto_fd
  fdnull (bool temp = false) noexcept;
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
  LIBBUTL_EXPORT fdpipe
  fdopen_pipe (fdopen_mode = fdopen_mode::none);
}

#include <libbutl/fdstream.ixx>

#endif // LIBBUTL_FDSTREAM_HXX