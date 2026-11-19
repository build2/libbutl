// file      : libbutl-archive/archive.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl-archive/archive.hxx>

#ifdef _WIN32
#  include <libbutl/win32-utility.hxx> // Pre-include <windows.h> lean & mean.
#endif

#include "archive.h"
#include "archive_entry.h"

#include <new>      // std::bad_alloc
#include <cerrno>
#include <chrono>
#include <utility>  // std::move()
#include <sstream>

#include <libbutl/utility.hxx>

using namespace std;

namespace butl
{
  // archive_error
  //
  static inline string
  format_error (archive* a, const char* what)
  {
    string r;

    if (what != nullptr)
      r = what;

    const char* e (archive_error_string (a));

    if (e != nullptr && *e != '\0')
    {
      if (what != nullptr)
        r += ": ";

      // Note that libarchive errors are typically capitalized.
      //
      if (*e >= 'A' && *e <= 'Z')
      {
        r += *e + 32;
        ++e;
      }

      r += e;
    }

    if (r.empty ())
      r = "unknown error";

    return r;
  }

  archive_error::
  archive_error (path f, std::string d)
      : runtime_error (d),
        file (move (f)), description (move (d))
  {
  }

  archive_error::
  archive_error (path f, archive* a, const char* w)
      : runtime_error (format_error (a, w)),
        file (move (f)), description (format_error (a, w))
  {
  }

  // archive_deleter
  //
  void archive_writer::archive_deleter::
  operator() (archive* a)
  {
    // Note that archive_write_free() will call archive_write_close() if
    // necessary (but will ignore any errors).
    //
    if (a != nullptr)
      archive_write_free (a);
  }

  // archive
  //
  archive_writer::
  ~archive_writer () = default;

  streamsize archive_writer::
  write_callback (archive* a, void* data, const void* buf, size_t size)
  {
    archive_writer& o (*static_cast<archive_writer*> (data));

    streamsize r (fdwrite (o.fd_.get (), buf, size));

    if (r < 0)
    {
      // Do as the file_write() callback in libarchive.
      //
      archive_set_error (a, errno, "Write error");
      return -1;
    }

    return r;
  }

  archive_writer::
  archive_writer (path f)
      : file_ (move (f)),
        archive_ (archive_write_new ())
  {
    if (archive_ == nullptr)
      throw bad_alloc ();

    archive* a (archive_.get ());

    if (archive_write_add_filter_gzip (a) != ARCHIVE_OK)
      throw archive_error (file_.path, a, "unable to set archive gzip filter");

    if (archive_write_set_format_ustar (a) != ARCHIVE_OK)
      throw archive_error (file_.path, a, "unable to set archive ustar format");

    if (archive_write_set_filter_option (
          a, "gzip", "timestamp", nullptr) != ARCHIVE_OK)
      throw archive_error (
        file_.path, a, "unable to disable archive gzip timestamp");

    if (archive_write_set_filter_option (
          a, "gzip", "compression-level", "9") != ARCHIVE_OK)
      throw archive_error (
        file_.path, a, "unable to set archive gzip compression level");

    try
    {
      fd_ = fdopen (file_.path, (fdopen_mode::out      |
                                 fdopen_mode::create   |
                                 fdopen_mode::truncate |
                                 fdopen_mode::binary));
    }
    catch (const ios::failure& e)
    {
      ostringstream oss;
      oss << "unable to open archive: " << e;
      throw archive_error (file_.path, oss.str ());
    }

    if (archive_write_open2 (
          a,
          this,
          nullptr,
          [] (archive* a, void* d, const void* b, size_t n) -> la_ssize_t
          {
            return static_cast<la_ssize_t> (
              archive_writer::write_callback (a, d, b, n));
          },
          nullptr,
          nullptr) != ARCHIVE_OK)
      throw archive_error (file_.path, a, "unable to open archive");
  }

  auto_rmfile archive_writer::
  close ()
  {
    archive* a (archive_.get ());

    if (archive_write_close (a) != ARCHIVE_OK)
      throw archive_error (file_.path, a, "unable to close archive");

    try
    {
      fd_.close ();
    }
    catch (const ios::failure& e)
    {
      ostringstream oss;
      oss << "unable to close archive: " << e;
      throw archive_error (file_.path, oss.str ());
    }

    return move (file_);
  }

  struct archive_entry_deleter
  {
    void operator() (archive_entry* e)
    {
      archive_entry_free (e);
    }
  };

  void archive_writer::
  add_file_entry (const path& fpath,
                  bool text,
                  const path& apath,
                  unsigned short mode,
                  timestamp mt)
  {
    archive* a (archive_.get ());

    if (apath.absolute ())
      throw archive_error (apath, "archive entry path is absolute");

    string astr;
    try
    {
      astr = apath.posix_string ();
    }
    catch (const invalid_path&)
    {
      throw archive_error (
        apath, "archive entry path cannot be represented as POSIX path");
    }

    // Must exists and be a regular file.
    //
    pair<bool, entry_stat> p (path_entry (fpath));

    if (!p.first)
      throw archive_error (fpath, "archive entry does not exist on filesystem");

    if (p.second.type != entry_type::regular)
      throw archive_error (
        fpath, "archive entry is not a regular file on filesystem");

    auto open_fpath = [&fpath, a, text] () -> auto_fd
    {
      auto_fd fd;
      try
      {
        fd = fdopen (fpath,
                     (fdopen_mode::in |
                      (text ? fdopen_mode::none : fdopen_mode::binary)));
      }
      catch (const ios::failure& e)
      {
        archive_write_fail (a); // Mark the archive object as failed.

        ostringstream oss;
        oss << "unable to open archive entry on filesystem: " << e;
        throw archive_error (fpath, oss.str ());
      }

      return fd;
    };

    char buf[fdstreambuf::buffer_size];

#ifdef _WIN32
    // With the CRLF translation the returned size will not match what will be
    // written. So we recalculate it.
    //
    if (text && p.second.size != 0)
    {
      p.second.size = 0;

      // Note: similar code to below.
      //
      auto_fd fd (open_fpath ());

      for (streamsize n;;)
      {
        n = fdread (fd.get (), buf, sizeof (buf));

        if (n < 0)
        {
          archive_write_fail (a); // Mark the archive object as failed.
          throw archive_error (
            fpath, "unable to read archive entry on filesystem");
        }

        if (n == 0)
          break;

        p.second.size += static_cast<size_t> (n);
      }
    }
#endif

    unique_ptr<archive_entry, archive_entry_deleter> ae (archive_entry_new ());

    if (ae == nullptr)
      throw bad_alloc ();

    archive_entry* e (ae.get ());

    archive_entry_set_pathname (e, astr.c_str ());
    archive_entry_set_size     (e, p.second.size);
    archive_entry_set_filetype (e, AE_IFREG);
    archive_entry_set_perm     (e, static_cast<mode_t> (mode));

    {
      using namespace chrono;

      time_t s (system_clock::to_time_t (mt));
      timestamp mts (system_clock::from_time_t (s));
      nanoseconds ns (duration_cast<nanoseconds> (mt - mts));

      archive_entry_set_mtime (e, s, static_cast<long> (ns.count ()));
    }

    // Note that we don't expect to get ARCHIVE_WARN or ARCHIVE_RETRY.
    //
    if (archive_write_header (a, e) != ARCHIVE_OK)
      throw archive_error (
        file_.path, a, "unable to write archive entry header");

    if (p.second.size != 0)
    {
      // Note: similar code to above.
      //
      auto_fd fd (open_fpath ());

      for (streamsize n;;)
      {
        n = fdread (fd.get (), buf, sizeof (buf));

        if (n < 0)
        {
          archive_write_fail (a); // Mark the archive object as failed.
          throw archive_error (
            fpath, "unable to read archive entry on filesystem");
        }

        if (n == 0)
          break;

        if (archive_write_data (a, buf, static_cast<size_t> (n)) < 0)
          throw archive_error (
            file_.path, a, "unable to write archive entry data");
      }
    }
  }
}
