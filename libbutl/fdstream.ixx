// file      : libbutl/fdstream.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <cassert>

namespace butl
{
  // auto_fd
  //
  inline void auto_fd::
  reset (int fd) noexcept
  {
    if (fd_ >= 0)
      fdclose (fd_); // Don't check for an error as not much we can do here.

    fd_ = fd;
  }

  inline auto_fd& auto_fd::
  operator= (auto_fd&& fd) noexcept
  {
    reset (fd.release ());
    return *this;
  }

  inline auto_fd::
  ~auto_fd () noexcept
  {
    reset ();
  }

  // fdstreambuf
  //
  inline fdstreambuf::
  fdstreambuf (auto_fd&& fd, std::uint64_t pos)
  {
    if (fd.get () >= 0)
      open (std::move (fd), pos);
  }

  inline auto_fd fdstreambuf::
  release ()
  {
    return std::move (fd_);
  }

  // fdstream_base
  //
  inline fdstream_base::
  fdstream_base (auto_fd&& fd, std::uint64_t pos)
      : buf_ (std::move (fd), pos)
  {
  }

  // ifdstream
  //
  inline ifdstream::
  ifdstream (auto_fd&& fd, iostate e, std::uint64_t pos)
      : fdstream_base (std::move (fd), pos), std::istream (&buf_)
  {
    assert (e & badbit);
    exceptions (e);
  }

  inline ifdstream::
  ifdstream (iostate e)
      : ifdstream (auto_fd (), e)
  {
  }

  inline ifdstream::
  ifdstream (auto_fd&& fd, fdstream_mode m, iostate e, std::uint64_t pos)
      : fdstream_base (std::move (fd), m, pos),
        std::istream (&buf_),
        skip_ ((m & fdstream_mode::skip) == fdstream_mode::skip)
  {
    assert (e & badbit);
    exceptions (e);
  }

  inline ifdstream::
  ifdstream (const std::string& f, openmode m, iostate e)
      : ifdstream (f.c_str (), m, e)
  {
  }

  inline ifdstream::
  ifdstream (const path& f, openmode m, iostate e)
      : ifdstream (f.string (), m, e)
  {
  }

  inline ifdstream::
  ifdstream (const std::string& f, fdopen_mode m, iostate e)
      : ifdstream (f.c_str (), m, e)
  {
  }

  inline ifdstream::
  ifdstream (const path& f, fdopen_mode m, iostate e)
      : ifdstream (f.string (), m, e)
  {
  }

  inline ifdstream::
  ifdstream (const char* f, iostate e)
      : ifdstream (f, in, e)
  {
  }

  inline ifdstream::
  ifdstream (const std::string& f, iostate e)
      : ifdstream (f, in, e)
  {
  }

  inline ifdstream::
  ifdstream (const path& f, iostate e)
      : ifdstream (f, in, e)
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

  inline auto_fd ifdstream::
  release ()
  {
    return buf_.release ();
  }

  inline std::string ifdstream::
  read_text ()
  {
    std::string s;

    // Note that the eof check is important: if the stream is at eof (empty
    // file) then getline() will fail.
    //
    if (peek () != traits_type::eof ())
      butl::getline (*this, s, '\0'); // Hidden by istream::getline().

    return s;
  }

  inline std::vector<char> ifdstream::
  read_binary ()
  {
    // @@ TODO: surely there is a more efficient way! See sha256!

    std::vector<char> v (std::istreambuf_iterator<char> (*this),
                         std::istreambuf_iterator<char> ());
    return v;
  }

  // ofdstream
  //
  inline ofdstream::
  ofdstream (auto_fd&& fd, iostate e, std::uint64_t pos)
      : fdstream_base (std::move (fd), pos), std::ostream (&buf_)
  {
    assert (e & badbit);
    exceptions (e);
  }

  inline ofdstream::
  ofdstream (iostate e)
      : ofdstream (auto_fd (), e)
  {
  }

  inline ofdstream::
  ofdstream (auto_fd&& fd, fdstream_mode m, iostate e, std::uint64_t pos)
      : fdstream_base (std::move (fd), m, pos), std::ostream (&buf_)
  {
    assert (e & badbit);
    exceptions (e);
  }

  inline ofdstream::
  ofdstream (const std::string& f, openmode m, iostate e)
      : ofdstream (f.c_str (), m, e)
  {
  }

  inline ofdstream::
  ofdstream (const path& f, openmode m, iostate e)
      : ofdstream (f.string (), m, e)
  {
  }

  inline ofdstream::
  ofdstream (const std::string& f, fdopen_mode m, iostate e)
      : ofdstream (f.c_str (), m, e)
  {
  }

  inline ofdstream::
  ofdstream (const path& f, fdopen_mode m, iostate e)
      : ofdstream (f.string (), m, e)
  {
  }

  inline ofdstream::
  ofdstream (const char* f, iostate e)
      : ofdstream (f, out, e)
  {
  }

  inline ofdstream::
  ofdstream (const std::string& f, iostate e)
      : ofdstream (f, out, e)
  {
  }

  inline ofdstream::
  ofdstream (const path& f, iostate e)
      : ofdstream (f, out, e)
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

  inline auto_fd ofdstream::
  release ()
  {
    if (is_open ())
      flush ();

    return buf_.release ();
  }

  // fdopen()
  //
  inline auto_fd
  fdopen (const std::string& f, fdopen_mode m, permissions p)
  {
    return fdopen (f.c_str (), m, p);
  }

  inline auto_fd
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

  // std*_fdmode()
  //
  inline fdstream_mode
  stdin_fdmode (fdstream_mode m)
  {
    return fdmode (stdin_fd (), m);
  }

  inline fdstream_mode
  stdout_fdmode (fdstream_mode m)
  {
    return fdmode (stdout_fd (), m);
  }

  inline fdstream_mode
  stderr_fdmode (fdstream_mode m)
  {
    return fdmode (stderr_fd (), m);
  }

  // fdselect
  //
  // Implement fdselect() function templates in terms of their milliseconds
  // specialization.
  //
  template <>
  LIBBUTL_SYMEXPORT std::pair<std::size_t, std::size_t>
  fdselect (fdselect_set&, fdselect_set&, const std::chrono::milliseconds&);

  template <typename R, typename P>
  inline std::pair<std::size_t, std::size_t>
  fdselect (fdselect_set& ifds,
            fdselect_set& ofds,
            const std::chrono::duration<R, P>& timeout)
  {
    using namespace std::chrono;
    return fdselect (ifds, ofds, duration_cast<milliseconds> (timeout));
  }
}
