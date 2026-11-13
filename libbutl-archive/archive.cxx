// file      : libbutl-archive/archive.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl-archive/archive.hxx>

#ifdef _WIN32
#  include <libbutl/win32-utility.hxx> // Pre-include <windows.h> lean & mean.
#endif

#include "archive.h"
#include "archive_entry.h"

#include <cassert>

#include <libbutl/fdstream.hxx>
#include <libbutl/filesystem.hxx>

using namespace std;

namespace butl
{
  void
  archive (const char* af, const char* ef, timestamp et)
  {
    pair<bool, entry_stat> p (path_entry (ef));

    //  Must exists and be a regular file.
    //
    assert (p.first && p.second.type == entry_type::regular);

    ofdstream ofs (af);

    ::archive* a (archive_write_new ());

    archive_write_add_filter_gzip (a);
    archive_write_set_format_ustar (a);

    archive_write_set_options (a, "gzip:!timestamp,gzip:compression-level=9");

    archive_write_open2 (
      a,
      &ofs,
      nullptr,
      [] (::archive*, void* data, const void* buf, size_t n) -> la_ssize_t
      {
        // @@ Should probably just use fdwrite() without exceptions, etc.

        ofdstream& ofs (*static_cast<ofdstream*> (data));
        ofs.write (static_cast<const char*> (buf), n);
        return n;
      },
      nullptr,
      nullptr);

    {
      archive_entry* e (archive_entry_new ());

      archive_entry_set_pathname (e, ef);
      archive_entry_set_size     (e, p.second.size);
      archive_entry_set_filetype (e, AE_IFREG);
      archive_entry_set_perm     (e, 0644);

      {
        using namespace chrono;

        time_t s (system_clock::to_time_t (et));
        timestamp ets (system_clock::from_time_t (s));
        nanoseconds ns (duration_cast<nanoseconds> (et - ets));

        archive_entry_set_mtime (e, s, static_cast<long> (ns.count ()));
      }

      archive_write_header (a, e);

      {
        auto_fd fd (fdopen (ef, fdopen_mode::in));

        // @@ What if file size is 0, do we still need to call
        //    archive_write_data()?

        char b[1024];

        for (streamsize n;;)
        {
          n = fdread (fd.get (), b, sizeof (b));
          assert (n >= 0);

          if (n == 0)
            break;

          archive_write_data (a, b, static_cast<size_t> (n));
        }

        fd.close ();
        archive_entry_free (e);
      }
    }

    archive_write_close (a);
    ofs.close ();

    archive_write_free (a);
  }
}
