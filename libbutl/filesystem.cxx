// file      : libbutl/filesystem.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/filesystem.hxx>

#include <errno.h> // errno, E*

#ifndef _WIN32
#  include <stdio.h>     // rename()
#  include <limits.h>    // PATH_MAX
#  include <dirent.h>    // struct dirent, *dir()
#  include <unistd.h>    // symlink(), link(), stat(), rmdir(), unlink()
#  include <sys/time.h>  // utimes()
#  include <sys/types.h> // stat
#  include <sys/stat.h>  // stat(), lstat(), S_I*, mkdir(), chmod()
#else
#  include <libbutl/win32-utility.hxx>

#  include <io.h>        // _unlink(), _chmod()
#  include <direct.h>    // _mkdir(), _rmdir()
#  include <winioctl.h>  // FSCTL_SET_REPARSE_POINT
#  include <sys/types.h> // _stat
#  include <sys/stat.h>  // _stat(), S_I*

#  ifdef _MSC_VER // Unlikely to be fixed in newer versions.
#    define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#    define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#    define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#  endif

#  include <cwchar>      // mbsrtowcs(), wcsrtombs(), mbstate_t
#  include <cstring>     // strncmp()
#  include <type_traits> // is_same
#endif

#include <chrono>
#include <vector>
#include <memory>       // unique_ptr
#include <cassert>
#include <algorithm>    // find(), copy()
#include <system_error>

#include <libbutl/path.hxx>
#include <libbutl/utility.hxx>      // throw_generic_error()
#include <libbutl/fdstream.hxx>
#include <libbutl/small-vector.hxx>

#ifndef _WIN32
#  ifndef PATH_MAX
#    define PATH_MAX 4096
#  endif
#endif

using namespace std;

namespace butl
{
#ifdef _WIN32
  using namespace win32;
#endif

  bool
  file_exists (const char* p, bool fl, bool ie)
  {
    auto pe (path_entry (p, fl, ie));
    return pe.first && (pe.second.type == entry_type::regular ||
                        (!fl && pe.second.type == entry_type::symlink));
  }

  bool
  entry_exists (const char* p, bool fl, bool ie)
  {
    return path_entry (p, fl, ie).first;
  }

  bool
  dir_exists (const char* p, bool ie)
  {
    auto pe (path_entry (p, true /* follow_symlinks */, ie));
    return pe.first && pe.second.type == entry_type::directory;
  }

#ifndef _WIN32

  pair<bool, entry_stat>
  path_entry (const char* p, bool fl, bool ie)
  {
    struct stat s;
    if ((fl ? stat (p, &s) : lstat (p, &s)) != 0)
    {
      if (errno == ENOENT || errno == ENOTDIR || ie)
        return make_pair (false, entry_stat {entry_type::unknown, 0});
      else
        throw_generic_error (errno);
    }

    auto m (s.st_mode);
    entry_type t (entry_type::unknown);

    if (S_ISREG (m))
      t = entry_type::regular;
    else if (S_ISDIR (m))
      t = entry_type::directory;
    else if (S_ISLNK (m))
      t = entry_type::symlink;
    else if (S_ISBLK (m) || S_ISCHR (m) || S_ISFIFO (m) || S_ISSOCK (m))
      t = entry_type::other;

    return make_pair (true, entry_stat {t, static_cast<uint64_t> (s.st_size)});
  }

  permissions
  path_permissions (const path& p)
  {
    struct stat s;
    if (stat (p.string ().c_str (), &s) != 0)
      throw_generic_error (errno);

    return static_cast<permissions> (s.st_mode &
                                     (S_IRWXU | S_IRWXG | S_IRWXO));
  }

  void
  path_permissions (const path& p, permissions f)
  {
    if (chmod (p.string ().c_str (), static_cast<mode_t> (f)) == -1)
      throw_generic_error (errno);
  }

  // Figuring out whether we have the nanoseconds in struct stat. Some
  // platforms (e.g., FreeBSD), may provide some "compatibility" #define's,
  // so use the second argument to not end up with the same signatures.
  //
  template <typename S>
  static inline constexpr auto
  mnsec (const S* s, bool) -> decltype(s->st_mtim.tv_nsec)
  {
    return s->st_mtim.tv_nsec; // POSIX (GNU/Linux, Solaris).
  }

  template <typename S>
  static inline constexpr auto
  mnsec (const S* s, int) -> decltype(s->st_mtimespec.tv_nsec)
  {
    return s->st_mtimespec.tv_nsec; // *BSD, MacOS.
  }

  template <typename S>
  static inline constexpr auto
  mnsec (const S* s, float) -> decltype(s->st_mtime_n)
  {
    return s->st_mtime_n; // AIX 5.2 and later.
  }

  // Things are not going to end up well with only seconds resolution so
  // let's make it a compile error.
  //
  // template <typename S>
  // static inline constexpr int
  // mnsec (...) {return 0;}

  template <typename S>
  static inline constexpr auto
  ansec (const S* s, bool) -> decltype(s->st_atim.tv_nsec)
  {
    return s->st_atim.tv_nsec; // POSIX (GNU/Linux, Solaris).
  }

  template <typename S>
  static inline constexpr auto
  ansec (const S* s, int) -> decltype(s->st_atimespec.tv_nsec)
  {
    return s->st_atimespec.tv_nsec; // *BSD, MacOS.
  }

  template <typename S>
  static inline constexpr auto
  ansec (const S* s, float) -> decltype(s->st_atime_n)
  {
    return s->st_atime_n; // AIX 5.2 and later.
  }

  // template <typename S>
  // static inline constexpr int
  // ansec (...) {return 0;}

  static inline entry_time
  entry_tm (const struct stat& s) noexcept
  {
    auto tm = [] (time_t sec, auto nsec) -> timestamp
    {
      return system_clock::from_time_t (sec) +
        chrono::duration_cast<duration> (chrono::nanoseconds (nsec));
    };

    return {tm (s.st_mtime, mnsec<struct stat> (&s, true)),
            tm (s.st_atime, ansec<struct stat> (&s, true))};
  }

  // Return the modification and access times of a regular file or directory.
  //
  static entry_time
  entry_tm (const char* p, bool dir)
  {
    struct stat s;
    if (stat (p, &s) != 0)
    {
      if (errno == ENOENT || errno == ENOTDIR)
        return {timestamp_nonexistent, timestamp_nonexistent};
      else
        throw_generic_error (errno);
    }

    if (dir ? !S_ISDIR (s.st_mode) : !S_ISREG (s.st_mode))
      return {timestamp_nonexistent, timestamp_nonexistent};

    return entry_tm (s);
  }

  // Set the modification and access times for a regular file or directory.
  //
  static void
  entry_tm (const char* p, const entry_time& t, bool dir)
  {
    // @@ Perhaps fdopen/fstat/futimens would be more efficient (also below)?

    struct stat s;
    if (stat (p, &s) != 0)
      throw_generic_error (errno);

    // If the entry is of the wrong type, then let's pretend that it doesn't
    // exists. In other words, the entry of the required type doesn't exist.
    //
    if (dir ? !S_ISDIR (s.st_mode) : !S_ISREG (s.st_mode))
      throw_generic_error (ENOENT);

    // Note: timeval has the microsecond resolution.
    //
    auto tm = [] (timestamp t, time_t sec, auto nsec) -> timeval
    {
      using usec_type = decltype (timeval::tv_usec);

      if (t == timestamp_nonexistent)
        return {sec, static_cast<usec_type> (nsec / 1000)};

      uint64_t usec (chrono::duration_cast<chrono::microseconds> (
                       t.time_since_epoch ()).count ());

      return {static_cast<time_t> (usec / 1000000),     // Seconds.
              static_cast<usec_type> (usec % 1000000)}; // Microseconds.
    };

    timeval times[2];
    times[0] = tm (t.access, s.st_atime, ansec<struct stat> (&s, true));
    times[1] = tm (t.modification, s.st_mtime, mnsec<struct stat> (&s, true));

    if (utimes (p, times) != 0)
      throw_generic_error (errno);
  }

#else

  static inline bool
  error_file_not_found (DWORD ec) noexcept
  {
    return ec == ERROR_FILE_NOT_FOUND ||
           ec == ERROR_PATH_NOT_FOUND ||
           ec == ERROR_INVALID_NAME   ||
           ec == ERROR_INVALID_DRIVE  ||
           ec == ERROR_BAD_PATHNAME   ||
           ec == ERROR_BAD_NETPATH;
  }

  static inline bool
  readonly (DWORD a) noexcept
  {
    return (a & FILE_ATTRIBUTE_READONLY) != 0;
  }

  static inline bool
  reparse_point (DWORD a) noexcept
  {
    return (a & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
  }

  // Return true for both directories and directory-type reparse points.
  //
  static inline bool
  directory (DWORD a) noexcept
  {
    return (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
  }

  // Return true for regular directories.
  //
  static inline bool
  regular_directory (DWORD a) noexcept
  {
    return directory (a) && !reparse_point (a);
  }

  // Return true for directory-type reparse points.
  //
  static inline bool
  directory_reparse_point (DWORD a) noexcept
  {
    return directory (a) && reparse_point (a);
  }

  static inline bool
  directory_reparse_point (const char* p) noexcept
  {
    DWORD a (GetFileAttributesA (p));
    return a != INVALID_FILE_ATTRIBUTES && directory_reparse_point (a);
  }

  // Open a filesystem entry for reading and optionally writing its
  // meta-information and return the entry handle and meta-information if the
  // path refers to an existing entry and nullhandle otherwise. Underlying OS
  // errors are reported by throwing std::system_error, unless ignore_error is
  // true in which case nullhandle is returned. In the latter case the error
  // code can be obtained by calling GetLastError().
  //
  static inline pair<win32::auto_handle, BY_HANDLE_FILE_INFORMATION>
  entry_info_handle (const char* p,
                     bool write,
                     bool follow_reparse_points,
                     bool ie = false)
  {
    // Open the entry for reading/writing its meta-information. Follow reparse
    // points.
    //
    // Note that the FILE_READ_ATTRIBUTES flag is not required.
    //
    auto_handle h (
      CreateFile (p,
                  write ? FILE_WRITE_ATTRIBUTES : 0,
                  0,
                  nullptr,
                  OPEN_EXISTING,
                  FILE_FLAG_BACKUP_SEMANTICS | // Required for a directory.
                  (follow_reparse_points ? 0 : FILE_FLAG_OPEN_REPARSE_POINT),
                  nullptr));

    if (h == nullhandle)
    {
      DWORD ec;
      if (ie || error_file_not_found (ec = GetLastError ()))
        return make_pair (nullhandle, BY_HANDLE_FILE_INFORMATION ());

      throw_system_error (ec);
    }

    BY_HANDLE_FILE_INFORMATION r;
    if (!GetFileInformationByHandle (h.get (), &r))
    {
      if (ie)
        return make_pair (nullhandle, BY_HANDLE_FILE_INFORMATION ());

      throw_system_error (GetLastError ());
    }

    return make_pair (move (h), r);
  }

  // Return a flag indicating whether the path is to an existing filesystem
  // entry and its meta-information if so.
  //
  static inline pair<bool, BY_HANDLE_FILE_INFORMATION>
  path_entry_handle_info (const char* p,
                          bool follow_reparse_points,
                          bool ie = false)
  {
    pair<auto_handle, BY_HANDLE_FILE_INFORMATION> hi (
      entry_info_handle (p, false /* write */, follow_reparse_points, ie));

    if (hi.first == nullhandle)
      return make_pair (false, BY_HANDLE_FILE_INFORMATION ());

    if (!ie)
      hi.first.close (); // Checks for error.

    return make_pair (true, hi.second);
  }

  static inline pair<bool, BY_HANDLE_FILE_INFORMATION>
  path_entry_handle_info (const path& p, bool fr, bool ie = false)
  {
    return path_entry_handle_info (p.string ().c_str (), fr, ie);
  }

  // Return a flag indicating whether the path is to an existing filesystem
  // entry and its extended attributes if so. Don't follow reparse points.
  //
  static inline pair<bool, WIN32_FILE_ATTRIBUTE_DATA>
  path_entry_info (const char* p, bool ie = false)
  {
    WIN32_FILE_ATTRIBUTE_DATA r;
    if (!GetFileAttributesExA (p, GetFileExInfoStandard, &r))
    {
      DWORD ec;
      if (ie || error_file_not_found (ec = GetLastError ()))
        return make_pair (false, WIN32_FILE_ATTRIBUTE_DATA ());

      throw_system_error (ec);
    }

    return make_pair (true, r);
  }

  static inline pair<bool, WIN32_FILE_ATTRIBUTE_DATA>
  path_entry_info (const path& p, bool ie = false)
  {
    return path_entry_info (p.string ().c_str (), ie);
  }

  // Reparse point data.
  //
  struct symlink_buf
  {
    WORD substitute_name_offset;
    WORD substitute_name_length;
    WORD print_name_offset;
    WORD print_name_length;

    ULONG flags;

    // Reserve space for the target path, target print name and their trailing
    // NULL characters.
    //
    wchar_t buf[2 * _MAX_PATH + 2];
  };

  struct mount_point_buf
  {
    WORD substitute_name_offset;
    WORD substitute_name_length;
    WORD print_name_offset;
    WORD print_name_length;

    // Reserve space for the target path, target print name and their trailing
    // NULL characters.
    //
    wchar_t buf[2 * _MAX_PATH + 2];
  };

  // The original type is REPARSE_DATA_BUFFER structure.
  //
  struct reparse_point_buf
  {
    // Header.
    //
    DWORD tag;
    WORD  data_length;
    WORD  reserved = 0;

    union
    {
      symlink_buf     symlink;
      mount_point_buf mount_point;
    };

    reparse_point_buf (DWORD t): tag (t) {}
    reparse_point_buf () = default;
  };

  // Try to read and parse the reparse point data at the specified path and
  // return the following values based on the reparse point existence/type:
  //
  // doesn't exist                 - {entry_type::unknown, <empty-path>}
  // symlink or junction           - {entry_type::symlink, <target-path>}
  // some other reparse point type - {entry_type::other,   <empty-path>}
  //
  // Underlying OS errors are reported by throwing std::system_error, unless
  // ignore_error is true in which case entry_type::unknown is returned. In
  // the later case the error code can be obtained by calling GetLastError().
  //
  static pair<entry_type, path>
  reparse_point_entry (const char* p, bool ie = false)
  {
    auto_handle h (CreateFile (p,
                               FILE_READ_EA,
                               0,
                               nullptr,
                               OPEN_EXISTING,
                               FILE_FLAG_BACKUP_SEMANTICS |
                               FILE_FLAG_OPEN_REPARSE_POINT,
                               nullptr));

    if (h == nullhandle)
    {
      DWORD ec;
      if (ie || error_file_not_found (ec = GetLastError ()))
        return make_pair (entry_type::unknown, path ());

      throw_system_error (ec);
    }

    reparse_point_buf rp;

    // Note that the wcsrtombs() function used for the path conversion
    // requires the source string to be NULL-terminated. However, the reparse
    // point target path returned by the DeviceIoControl() call is not
    // NULL-terminated. Thus, we reserve space in the buffer for the
    // trailing NULL character, which the parse() lambda adds later prior to
    // the conversion.
    //
    DWORD n;
    if (!DeviceIoControl (
          h.get (),
          FSCTL_GET_REPARSE_POINT,
          nullptr,
          0,
          &rp,
          sizeof (rp) - sizeof (wchar_t), // Reserve for the NULL character.
          &n,                             // May not be NULL.
          nullptr))
    {
      if (ie)
        return make_pair (entry_type::unknown, path ());

      throw_system_error (GetLastError ());
    }

    if (!ie)
      h.close (); // Checks for error.

    // Try to parse a sub-string in the wide character buffer as a path. The
    // sub-string offset and length are expressed in bytes. Verify that the
    // resulting path is absolute if the absolute argument is true and is
    // relative otherwise (but is not empty). Return nullopt if anything goes
    // wrong.
    //
    // Expects the buffer to have a room for the terminating NULL character
    // (see above for details).
    //
    auto parse = [] (wchar_t* s,
                     size_t off,
                     size_t len,
                     bool absolute) -> optional<path>
    {
      // The offset and length must be divisible by the wide character size
      // without a remainder.
      //
      if (off % sizeof (wchar_t) != 0 || len % sizeof (wchar_t) != 0)
        return nullopt;

      off /= sizeof (wchar_t);
      len /= sizeof (wchar_t);

      // Add the trailing NULL character to the buffer, saving the character
      // it overwrites to restore it later.
      //
      size_t n (off + len);
      wchar_t c (s[n]);
      s[n] = L'\0';

      const wchar_t* from (s + off);
      char buf[_MAX_PATH + 1];

      // Zero-initialize the conversion state (and disambiguate with the
      // function declaration).
      //
      mbstate_t state ((mbstate_t ()));

      size_t r (wcsrtombs (buf, &from, sizeof (buf), &state));
      s[n] = c; // Restore the character overwritten by the NULL character.

      if (r == static_cast<size_t> (-1))
        return nullopt;

      if (from != nullptr) // Not enough space in the destination buffer.
        return nullopt;

      // The buffer contains an absolute path, distinguished by the special
      // prefix. For example: \??\C:\tmp.
      //
      bool abs (strncmp (buf, "\\??\\", 4) == 0);

      if (abs != absolute)
        return nullopt;

      // A prefix starting with the backslash but different from the '\??\'
      // denotes some special Windows path that we don't support. For example,
      // volume mount point paths that looks as follows:
      //
      // \\?\Volume{9a687afc-534d-11ea-9433-806e6f6e6963}
      //
      if (!abs && buf[0] == '\\')
        return nullopt;

      try
      {
        path r (buf + (abs ? 4 : 0));

        if (r.absolute () != abs || r.empty ())
          return nullopt;

        return r;
      }
      catch (const invalid_path&)
      {
        return nullopt;
      }
    };

    // Try to parse the reparse point data for the symlink-related tags.
    // Return entry_type::other on any parsing failure.
    //
    optional<path> r;
    switch (rp.tag)
    {
    case IO_REPARSE_TAG_SYMLINK:
      {
        symlink_buf& b (rp.symlink);

        // Note that the SYMLINK_FLAG_RELATIVE flag may not be defined at
        // compile-time so we pass its literal value (0x1).
        //
        r = parse (b.buf,
                   b.substitute_name_offset,
                   b.substitute_name_length,
                   (b.flags & 0x1) == 0);

        break;
      }
    case IO_REPARSE_TAG_MOUNT_POINT:
      {
        mount_point_buf& b (rp.mount_point);
        r = parse (b.buf,
                   b.substitute_name_offset,
                   b.substitute_name_length,
                   true /* absolute */);

        break;
      }
    }

    return r
           ? make_pair (entry_type::symlink, move (*r))
           : make_pair (entry_type::other,   path ());
  }

  static inline pair<entry_type, path>
  reparse_point_entry (const path& p, bool ie = false)
  {
    return reparse_point_entry (p.string ().c_str (), ie);
  }

  static inline timestamp
  to_timestamp (const FILETIME& t)
  {
    // Time in FILETIME is in 100 nanosecond "ticks" since "Windows epoch"
    // (1601-01-01T00:00:00Z). To convert it to "UNIX epoch"
    // (1970-01-01T00:00:00Z) we need to subtract 11644473600 seconds.
    //
    uint64_t nsec ((static_cast<uint64_t> (t.dwHighDateTime) << 32) |
                   t.dwLowDateTime);

    nsec -= 11644473600ULL * 10000000; // Now in UNIX epoch.
    nsec *= 100;                       // Now in nanoseconds.

    return timestamp (
      chrono::duration_cast<duration> (chrono::nanoseconds (nsec)));
  }

  static inline FILETIME
  to_filetime (timestamp t)
  {
    // Time in FILETIME is in 100 nanosecond "ticks" since "Windows epoch"
    // (1601-01-01T00:00:00Z). To convert "UNIX epoch" (1970-01-01T00:00:00Z)
    // to it we need to add 11644473600 seconds.
    //
    uint64_t ticks (chrono::duration_cast<chrono::nanoseconds> (
                      t.time_since_epoch ()).count ());

    ticks /= 100;                       // Now in 100 nanosecond "ticks".
    ticks += 11644473600ULL * 10000000; // Now in "Windows epoch".

    FILETIME r;
    r.dwHighDateTime = (ticks >> 32) & 0xFFFFFFFF;
    r.dwLowDateTime = ticks & 0xFFFFFFFF;
    return r;
  }

  // If the being returned entry type is regular or directory and et is not
  // NULL, then also save the entry modification and access times into the
  // referenced variable.
  //
  static inline pair<bool, entry_stat>
  path_entry (const char* p, bool fl, bool ie, entry_time* et)
  {
    // A path like 'C:', while being a root path in our terminology, is not as
    // such for Windows, that maintains current directory for each drive, and
    // so C: means the current directory on the drive C. This is not what we
    // mean here, so need to append the trailing directory separator in such a
    // case.
    //
    string d;
    if (path::traits_type::root (p))
    {
      d = string (p); // GCC bug #105329.
      d += path::traits_type::directory_separator;
      p = d.c_str ();
    }

    // Stat the entry not following reparse points.
    //
    pair<bool, WIN32_FILE_ATTRIBUTE_DATA> pi (path_entry_info (p, ie));

    if (!pi.first)
      return make_pair (false, entry_stat {entry_type::unknown, 0});

    auto entry_info = [et] (const auto& ei)
    {
      if (et != nullptr)
      {
        et->modification = to_timestamp (ei.ftLastWriteTime);
        et->access = to_timestamp (ei.ftLastAccessTime);
      }

      if (directory (ei.dwFileAttributes))
        return make_pair (true, entry_stat {entry_type::directory, 0});
      else
        return make_pair (
          true,
          entry_stat {entry_type::regular,
                      ((uint64_t (ei.nFileSizeHigh) << 32) | ei.nFileSizeLow)});
    };

    if (!reparse_point (pi.second.dwFileAttributes))
      return entry_info (pi.second);

    pair<entry_type, path> rp (reparse_point_entry (p, ie));

    if (rp.first == entry_type::symlink)
    {
      // If following symlinks is requested, then follow the reparse point and
      // return its target information. Otherwise, return the symlink entry
      // type.
      //
      if (fl)
      {
        pair<bool, BY_HANDLE_FILE_INFORMATION> pi (
          path_entry_handle_info (p, true /* follow_reparse_points */, ie));

        return pi.first
               ? entry_info (pi.second)
               : make_pair (false, entry_stat {entry_type::unknown, 0});
      }
      else
        return make_pair (true, entry_stat {entry_type::symlink, 0});
    }
    else if (rp.first == entry_type::unknown)
      return make_pair (false, entry_stat {entry_type::unknown, 0});
    else // entry_type::other
      return make_pair (true, entry_stat {entry_type::other, 0});
  }

  static inline pair<bool, entry_stat>
  path_entry (const path& p, bool fl, bool ie, entry_time* et)
  {
    return path_entry (p.string ().c_str (), fl, ie, et);
  }

  pair<bool, entry_stat>
  path_entry (const char* p, bool fl, bool ie)
  {
    return path_entry (p, fl, ie, nullptr /* entry_time */);
  }

  permissions
  path_permissions (const path& p)
  {
    // Let's optimize for the common case when the entry is not a reparse
    // point.
    //
    auto attr_to_perm = [] (const auto& pi) -> permissions
    {
      if (!pi.first)
        throw_generic_error (ENOENT);

      // On Windows a filesystem entry is always readable. Also there is no
      // notion of group/other permissions at OS level, so we extrapolate user
      // permissions to group/other permissions (as the _stat() function
      // does).
      //
      permissions r (permissions::ru | permissions::rg | permissions::ro);

      if (!readonly (pi.second.dwFileAttributes))
        r |= permissions::wu | permissions::wg | permissions::wo;

      return r;
    };

    pair<bool, WIN32_FILE_ATTRIBUTE_DATA> pi (path_entry_info (p));
    return !pi.first || !reparse_point (pi.second.dwFileAttributes)
           ? attr_to_perm (pi)
           : attr_to_perm (
               path_entry_handle_info (p, true /* follow_reparse_points */));
  }

  void
  path_permissions (const path& p, permissions f)
  {
    path tp (followsymlink (p));
    const char* t (tp.string ().c_str ());

    DWORD a (GetFileAttributesA (t));
    if (a == INVALID_FILE_ATTRIBUTES)
      throw_system_error (GetLastError ());

    DWORD na ((f & permissions::wu) != permissions::none // Writable?
              ? (a & ~FILE_ATTRIBUTE_READONLY)
              : (a | FILE_ATTRIBUTE_READONLY));

    if (na != a && !SetFileAttributesA (t, na))
      throw_system_error (GetLastError ());
  }

  // Return the modification and access times of a regular file or directory.
  //
  static entry_time
  entry_tm (const char* p, bool dir)
  {
    // Let's optimize for the common case when the entry is not a reparse
    // point.
    //
    auto attr_to_time = [dir] (const auto& pi) -> entry_time
    {
      // If the entry is of the wrong type, then let's pretend that it doesn't
      // exists.
      //
      if (!pi.first || directory (pi.second.dwFileAttributes) != dir)
        return entry_time {timestamp_nonexistent, timestamp_nonexistent};

      return entry_time {to_timestamp (pi.second.ftLastWriteTime),
                         to_timestamp (pi.second.ftLastAccessTime)};
    };

    pair<bool, WIN32_FILE_ATTRIBUTE_DATA> pi (path_entry_info (p));
    return !pi.first || !reparse_point (pi.second.dwFileAttributes)
           ? attr_to_time (pi)
           : attr_to_time (
               path_entry_handle_info (p, true /* follow_reparse_points */));
  }

  // Set the modification and access times for a regular file or directory.
  //
  static void
  entry_tm (const char* p, const entry_time& t, bool dir)
  {
    // See also touch_file() below.
    //
    pair<auto_handle, BY_HANDLE_FILE_INFORMATION> hi (
      entry_info_handle (p,
                         true /* write */,
                         true /* follow_reparse_points */));

    // If the entry is of the wrong type, then let's pretend that it doesn't
    // exist.
    //
    if (hi.first == nullhandle ||
        directory (hi.second.dwFileAttributes) != dir)
      throw_generic_error (ENOENT);

    auto tm = [] (timestamp t, FILETIME& ft) -> const FILETIME*
    {
      if (t == timestamp_nonexistent)
        return nullptr;

      ft = to_filetime (t);
      return &ft;
    };

    FILETIME at;
    FILETIME mt;
    if (!SetFileTime (hi.first.get (),
                      nullptr /* lpCreationTime */,
                      tm (t.access, at),
                      tm (t.modification, mt)))
      throw_system_error (GetLastError ());

    hi.first.close (); // Checks for error.
  }

#endif

#ifndef _WIN32
  bool
  touch_file (const path& p, bool create)
  {
    // utimes() (as well as utimensat()) have an unfortunate property of
    // succeeding if the path is a directory.
    //
    // @@ Perhaps fdopen/fstat/futimens would be more efficient (also above)?
    //
    pair<bool, entry_stat> pe (path_entry (p, true /* follow_symlinks */));

    if (pe.first)
    {
      // If the entry is of the wrong type, then let's pretend that it doesn't
      // exist.
      //
      if (pe.second.type == entry_type::regular)
      {
        if (utimes (p.string ().c_str (), nullptr) == -1)
          throw_generic_error (errno);

        return false;
      }
    }
    else if (create)
    {
      // Assuming the file access and modification times are set to the
      // current time automatically.
      //
      fdopen (p, fdopen_mode::out | fdopen_mode::create);
      return true;
    }

    throw_generic_error (ENOENT);
  }

#else

  bool
  touch_file (const path& p, bool create)
  {
    // We cannot use utime() on Windows since it has the seconds precision
    // even if we don't specify the time explicitly. So we use SetFileTime()
    // similar to entry_tm() above.
    //
    // Note also that on Windows there are two times: the precise time (which
    // is what we get with system_clock::now()) and what we will call the
    // "filesystem time", which can lag the precise time by as much as a
    // couple of milliseconds. To get the filesystem time one uses
    // GetSystemTimeAsFileTime(). We use this time here in order to keep
    // timestamps consistent with other operations where they are updated
    // implicitly.
    //
    pair<auto_handle, BY_HANDLE_FILE_INFORMATION> hi (
      entry_info_handle (p.string ().c_str (),
                         true /* write */,
                         true /* follow_reparse_points */));

    if (hi.first != nullhandle)
    {
      if (!directory (hi.second.dwFileAttributes))
      {
        FILETIME ft;
        GetSystemTimeAsFileTime (&ft); // Does not fail.

        if (!SetFileTime (hi.first.get (),
                          nullptr /* lpCreationTime */,
                          &ft,
                          &ft))
          throw_system_error (GetLastError ());

        hi.first.close (); // Checks for error.
        return false;
      }
    }
    else if (create)
    {
      // Assuming the file access and modification times are set to the
      // current time automatically.
      //
      fdopen (p, fdopen_mode::out | fdopen_mode::create);
      return true;
    }

    throw_generic_error (ENOENT);
  }
#endif

  mkdir_status
#ifndef _WIN32
  try_mkdir (const dir_path& p, mode_t m)
  {
    if (mkdir (p.string ().c_str (), m) != 0)
#else
  try_mkdir (const dir_path& p, mode_t)
  {
    if (_mkdir (p.string ().c_str ()) != 0)
#endif
    {
      int e (errno);

      // EEXIST means the path already exists but not necessarily as a
      // directory.
      //
      if (e == EEXIST && dir_exists (p))
        return mkdir_status::already_exists;
      else
        throw_generic_error (e);
    }

    return mkdir_status::success;
  }

  mkdir_status
  try_mkdir_p (const dir_path& p, mode_t m)
  {
    if (!p.root ())
    {
      dir_path d (p.directory ());

      if (!d.empty () && !dir_exists (d))
        try_mkdir_p (d, m);
    }

    return try_mkdir (p, m);
  }

  rmdir_status
  try_rmdir (const dir_path& p, bool ignore_error)
  {
    rmdir_status r (rmdir_status::success);
    const char* d (p.string ().c_str ());

#ifndef _WIN32
    int rr (rmdir (d));
#else
    // Note that we should only remove regular directories but not directory-
    // type reparse points (entry_type::{symlink,other} types). However,
    // _rmdir() removes both and thus we need to check the entry type
    // ourselves prior to the removal.
    //
    DWORD a (GetFileAttributesA (d));

    if (a == INVALID_FILE_ATTRIBUTES)
    {
      DWORD ec (GetLastError ());
      if (error_file_not_found (ec))
        return rmdir_status::not_exist;

      throw_system_error (ec);
    }

    // While at it, let's also check for non-directory not to end up with not
    // very specific EINVAL.
    //
    if (reparse_point (a) || !directory (a))
      throw_generic_error (ENOTDIR);

    // On Windows a directory with the read-only attribute can not be deleted.
    // Thus, we reset the attribute prior to removal and try to restore it if
    // the operation fails.
    //
    bool ro (readonly (a));

    // If we failed to reset the read-only attribute for any reason just try
    // to remove the entry and end up with the 'not found' or 'permission
    // denied' error code.
    //
    if (ro && !SetFileAttributes (d, a & ~FILE_ATTRIBUTE_READONLY))
      ro = false;

    int rr (_rmdir (d));

    // Restoring the attribute is unlikely to fail since we managed to reset
    // it earlier.
    //
    if (rr != 0 && ro)
      SetFileAttributes (d, a);

#endif

    if (rr != 0)
    {
      if (errno == ENOENT)
        r = rmdir_status::not_exist;
      else if (errno == ENOTEMPTY || errno == EEXIST)
        r = rmdir_status::not_empty;
      else if (!ignore_error)
        throw_generic_error (errno);
    }

    return r;
  }

  void
  rmdir_r (const dir_path& p, bool dir, bool ignore_error)
  {
    // An nftw()-based implementation (for platforms that support it)
    // might be a faster way.
    //
    // @@ Get rid of these try/catch clauses when ignore_error flag is
    //    implemented for dir_iterator() constructor.
    //
    try
    {
      for (const dir_entry& de: dir_iterator (p, dir_iterator::no_follow))
      {
        path ep (p / de.path ()); //@@ Would be good to reuse the buffer.

        if (de.ltype () == entry_type::directory)
          rmdir_r (path_cast<dir_path> (move (ep)), true, ignore_error);
        else
          try_rmfile (ep, ignore_error);
      }
    }
    catch (const system_error&)
    {
      if (!ignore_error)
        throw;
    }

    if (dir)
    {
      rmdir_status r (try_rmdir (p, ignore_error));

      if (r != rmdir_status::success && !ignore_error)
        throw_generic_error (r == rmdir_status::not_empty
                             ? ENOTEMPTY
                             : ENOENT);
    }
  }

  optional<rmfile_status>
  try_rmfile_maybe_ignore_error (const path& p, bool ignore_error)
  {
    rmfile_status r (rmfile_status::success);
    const char* f (p.string ().c_str ());

#ifndef _WIN32
    int ur (unlink (f));
#else
    int ur;

    // On Windows a file with the read-only attribute can not be deleted. This
    // can be the reason if the deletion fails with the 'permission denied'
    // error code. In such a case we just reset the attribute and repeat the
    // attempt. If the attempt fails, then we try to restore the attribute.
    //
    // Yet another reason for the 'permission denied' failure can be a
    // directory. We need to fail for a regular directory and retry using
    // _rmdir() for a directory-type reparse point.
    //
    // And also there are some unknown reasons for the 'permission denied'
    // failure (see mventry() for details). If that's the case, we will keep
    // trying to move the file for two seconds.
    //
    for (size_t i (0); i < 41; ++i)
    {
      // Sleep 50 milliseconds before the removal retry.
      //
      if (i != 0)
        Sleep (50);

      ur = _unlink (f);

      //@@ Should we check for SHARING_VIOLATION?

      if (ur != 0 && errno == EACCES)
      {
        DWORD a (GetFileAttributesA (f));
        if (a != INVALID_FILE_ATTRIBUTES)
        {
          if (regular_directory (a))
            break;

          bool ro (readonly (a));

          // Remove a directory-type reparse point as a directory.
          //
          bool dir (directory (a));

          if (ro || dir)
          {
            bool restore (ro &&
                          SetFileAttributes (f,
                                             a & ~FILE_ATTRIBUTE_READONLY));

            ur = dir ? _rmdir (f) : _unlink (f);

            // Restoring the attribute is unlikely to fail since we managed to
            // reset it earlier.
            //
            if (ur != 0 && restore)
              SetFileAttributes (f, a);
          }
        }
      }

      if (ur == 0 || errno != EACCES)
        break;
    }
#endif

    if (ur != 0)
    {
      // Strangely on Linux unlink() removes a dangling symlink but returns
      // ENOENT.
      //
      if (errno == ENOENT || errno == ENOTDIR)
        r = rmfile_status::not_exist;
      else if (!ignore_error)
        throw_generic_error (errno);
      else
        return nullopt;
    }

    return r;
  }

#ifndef _WIN32

  void
  mksymlink (const path& target, const path& link, bool)
  {
    if (symlink (target.string ().c_str (), link.string ().c_str ()) == -1)
      throw_generic_error (errno);
  }

  path
  readsymlink (const path& p)
  {
    char buf [PATH_MAX + 1];
    ssize_t r (readlink (p.string ().c_str (), buf, sizeof (buf)));

    if (r == -1)
      throw_generic_error (errno);

    if (static_cast<size_t> (r) == sizeof (buf))
      throw_generic_error (ENAMETOOLONG);

    buf[r] = '\0';

    try
    {
      return path (buf);
    }
    catch (const invalid_path&)
    {
      throw_generic_error (EINVAL);
    }
  }

  void
  mkhardlink (const path& target, const path& link, bool)
  {
    if (::link (target.string ().c_str (), link.string ().c_str ()) == -1)
      throw_generic_error (errno);
  }

#else

  void
  mksymlink (const path& target, const path& link, bool dir)
  {
    // Canonicalize the reparse point target path to make sure that Windows
    // API won't fail resolving it.
    //
    path tg (target);
    tg.canonicalize ();

    // Try to create a symbolic link without elevated privileges by passing
    // the new SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE flag. The flag is
    // new and it may not be defined at compile-time so we pass its literal
    // value (0x2).
    //
    // We may also be running on an earlier version of Windows (before 10
    // build 14972) that doesn't support it. The natural way to handle that
    // would have been to check the build of Windows that we are running on
    // but that turns out to not be that easy (requires a deprecated API,
    // specific executable manifest setup, a dance on one leg, and who knows
    // what else).
    //
    // So instead we are going to just make the call and if the result is the
    // invalid argument error, assume that the flag is not recognized. Except
    // that this error can also be returned if the link path or the target
    // path are invalid. So if we get this error, we also stat the two paths
    // to make sure we don't get the same error for them.
    //
    // Note that the CreateSymbolicLinkA() function is only available starting
    // with Windows Vista/Server 2008. To allow programs linked to libbutl to
    // run on earlier Windows versions we will link the API in run-time. We
    // also use the SYMBOLIC_LINK_FLAG_DIRECTORY flag literal value (0x1) as
    // it may not be defined at compile-time.
    //
    HMODULE kh (GetModuleHandle ("kernel32.dll"));
    if (kh != nullptr)
    {
      using func = BOOL (*) (const char*, const char*, DWORD);

      func f (function_cast<func> (GetProcAddress (kh,
                                                   "CreateSymbolicLinkA")));

      if (f != nullptr)
      {
        if (f (link.string ().c_str (),
               tg.string ().c_str (),
               0x2 | (dir ? 0x1 : 0)))
          return;

        // Note that ERROR_INVALID_PARAMETER means that either the function
        // doesn't recognize the SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
        // flag (that happens on elder systems) or the target or link paths are
        // invalid. Thus, we additionally check the paths to distinguish the
        // cases.
        //
        if (GetLastError () == ERROR_INVALID_PARAMETER)
        {
          auto invalid = [] (const path& p)
          {
            return GetFileAttributesA (p.string ().c_str ()) ==
                   INVALID_FILE_ATTRIBUTES &&
                   GetLastError () == ERROR_INVALID_PARAMETER;
          };

          if (invalid (tg) || invalid (link))
            throw_generic_error (EINVAL);
        }
      }
    }

    // Fallback to creating a junction if we failed to create a directory
    // symlink and bail out for a file symlink.
    //
    if (!dir)
      throw_generic_error (ENOSYS, "file symlinks not supported");

    // Create as a junction.
    //
    dir_path ld (path_cast<dir_path> (link));

    mkdir_status rs (try_mkdir (ld));

    if (rs == mkdir_status::already_exists)
      throw_generic_error (EEXIST);

    assert (rs == mkdir_status::success);

    // We need to be careful with the directory auto-removal since it is
    // recursive. So we must cancel it right after the reparse point target
    // is setup for the directory path.
    //
    auto_rmdir rm (ld);

    auto_handle h (CreateFile (ld.string ().c_str (),
                               GENERIC_WRITE,
                               0,
                               nullptr,
                               OPEN_EXISTING,
                               FILE_FLAG_BACKUP_SEMANTICS |
                               FILE_FLAG_OPEN_REPARSE_POINT,
                               nullptr));

    if (h == nullhandle)
      throw_system_error (GetLastError ());

    // Complete and normalize the target path.
    //
    if (tg.relative ())
    {
      tg = link.directory () / tg;

      if (tg.relative ())
        tg.complete ();
    }

    try
    {
      tg.normalize ();
    }
    catch (const invalid_path&)
    {
      throw_generic_error (EINVAL);
    }

    // Convert a path from the character string to the wide-character string
    // representation and return the resulting string length in characters
    // (first) and in bytes (second). Append the trailing NULL character to
    // the resulting string but exclude it from the length.
    //
    auto conv = [] (const string& from, wchar_t* to)
    {
      const char* s (from.c_str ());

      // Zero-initialize the conversion state (and disambiguate with the
      // function declaration).
      //
      mbstate_t state ((mbstate_t ()));
      size_t n (mbsrtowcs (to, &s, _MAX_PATH + 1, &state));

      if (n == static_cast<size_t> (-1))
        throw_generic_error (errno);

      if (s != nullptr) // Not enough space in the destination buffer.
        throw_generic_error (ENAMETOOLONG);

      return make_pair (n, static_cast<WORD> (n * sizeof (wchar_t)));
    };

    // We will fill the uninitialized members later.
    //
    reparse_point_buf rp (IO_REPARSE_TAG_MOUNT_POINT);
    mount_point_buf&  mp (rp.mount_point);

    // Decorate, convert and copy the junction target path (\??\C:\...) into
    // the wide-character buffer.
    //
    pair<size_t, WORD> np (conv ("\\??\\" + tg.string (), mp.buf));

    // Convert and copy the junction target print name (C:\...) into the
    // wide-character buffer.
    //
    pair<size_t, WORD> nn (conv (tg.string (), mp.buf + np.first + 1));

    // Fill the rest of the structure and setup the reparse point.
    //
    // The path offset and length, in bytes.
    //
    mp.substitute_name_offset = 0;
    mp.substitute_name_length = np.second;

    // The print name offset and length, in bytes.
    //
    mp.print_name_offset = np.second + sizeof (wchar_t);
    mp.print_name_length = nn.second;

    // The mount point reparse buffer size.
    //
    rp.data_length =
      4 * sizeof (WORD)            + // Size of *_name_* fields.
      np.second + sizeof (wchar_t) + // Path length, in bytes.
      nn.second + sizeof (wchar_t);  // Print name length, in bytes.

    DWORD r;
    if (!DeviceIoControl (
          h.get (),
          FSCTL_SET_REPARSE_POINT,
          &rp,
          sizeof (DWORD) + 2 * sizeof (WORD) + // Size of the header.
          rp.data_length,                      // Size of the mount point
          nullptr,                             // reparse buffer.
          0,
          &r,                                  // May not be NULL.
          nullptr))
      throw_system_error (GetLastError ());

    h.close (); // Checks for error.

    rm.cancel ();
  }

  path
  readsymlink (const path& p)
  {
    pair<entry_type, path> rp (reparse_point_entry (p));

    if (rp.first == entry_type::symlink)
      return move (rp.second);
    else if (rp.first == entry_type::unknown)
      throw_generic_error (ENOENT);
    else // entry_type::other
      throw_generic_error (EINVAL);
  }

  void
  mkhardlink (const path& target, const path& link, bool dir)
  {
    if (!dir)
    {
      if (!CreateHardLinkA (link.string ().c_str (),
                            target.string ().c_str (),
                            nullptr))
      {
        // If creation fails because hardlinks are not supported for the
        // specified target/link paths combination, then throw system_error of
        // the generic category with an approximate POSIX errno code. This way
        // the caller could recognize such a case and, for example, fallback
        // to the copy operation. For other error codes use the system
        // category.
        //
        DWORD ec (GetLastError ());
        switch (ec)
        {
          // Target and link are on different drives.
          //
        case ERROR_NOT_SAME_DEVICE: throw_generic_error (EXDEV);

          // Target and link are on the same drive, which doesn't support
          // hardlinks.
          //
        case ERROR_ACCESS_DENIED: throw_generic_error (EPERM);

          // Some other failure reason. Fallback to the system category.
          //
        default: throw_system_error (ec);
        }
      }
    }
    else
      throw_generic_error (ENOSYS, "directory hard links not supported");
  }
#endif

  pair<path, bool>
  try_followsymlink (const path& p)
  {
    path r (p);
    bool exists;

    for (size_t i (0);; ++i)
    {
      pair<bool, entry_stat> pe (path_entry (r));

      exists = pe.first;

      // Check if we reached the end of the symlink chain.
      //
      if (!exists || pe.second.type != entry_type::symlink)
        break;

      // Bail out if the symlink chain is too long or is a cycle.
      //
      // Note that there is no portable and easy way to obtain the
      // corresponding OS-level limit. However, the maximum chain length of 50
      // symlinks feels to be sufficient given that on Linux the maximum
      // number of followed symbolic links is 40 and on Windows the maximum
      // number of reparse points per path is 31.
      //
      if (i == 50)
        throw_generic_error (ELOOP);

      path tp (readsymlink (r));

      // Rebase a relative target path over the resulting path and reset the
      // resulting path to an absolute target path.
      //
      bool rel (tp.relative ());
      r = rel ? r.directory () / tp : move (tp);

      // Note that we could potentially optimize and delay the resulting path
      // normalization until the symlink chain is followed. However, this
      // imposes a risk that the path can become too long for Windows API to
      // properly handle it.
      //
      if (rel)
      try
      {
        r.normalize ();
      }
      catch (const invalid_path&)
      {
        throw_generic_error (EINVAL);
      }
    }

    return make_pair (move (r), exists);
  }

  rmfile_status
  try_rmsymlink (const path& link, bool, bool ie)
  {
    return try_rmfile (link, ie);
  }

  entry_type
  mkanylink (const path& target, const path& link, bool copy, bool rel)
  {
    using error = pair<entry_type, system_error>;

    const path& tp (rel ? target.relative (link.directory ()) : target);

    try
    {
      mksymlink (tp, link);
      return entry_type::symlink;
    }
    catch (system_error& e)
    {
      // Note that we are not guaranteed (here and below) that the
      // system_error exception is of the generic category.
      //
      if (e.code ().category () == generic_category ())
      {
        int c (e.code ().value ());
        if (c == ENOSYS || // Not implemented.
            c == EPERM)    // Not supported by the filesystem(s).
        {
          // Note that for hardlinking/copying we need to complete a relative
          // target using the link directory.
          //
          const path& ctp (tp.relative () ? link.directory () / tp : tp);

          try
          {
            // The target can be a symlink (or a symlink chain) with a
            // relative target that, unless the (final) symlink and the
            // hardlink are in the same directory, will result in a dangling
            // link.
            //
            mkhardlink (followsymlink (ctp), link);
            return entry_type::other;
          }
          catch (system_error& e)
          {
            if (copy && e.code ().category () == generic_category ())
            {
              int c (e.code ().value ());
              if (c == ENOSYS || // Not implemented.
                  c == EPERM  || // Not supported by the filesystem(s).
                  c == EXDEV)    // On different filesystems.
              {
                try
                {
                  cpfile (ctp, link);
                  return entry_type::regular;
                }
                catch (system_error& e)
                {
                  throw error (entry_type::regular, move (e));
                }
              }
            }

            throw error (entry_type::other, move (e));
          }
        }
      }

      throw error (entry_type::symlink, move (e));
    }
  }

  // For I/O operations cpfile() can throw ios_base::failure exception that is
  // not derived from system_error for old versions of g++ (as of 4.9). From
  // the other hand cpfile() must throw system_error only. Let's catch
  // ios_base::failure and rethrow as system_error in such a case.
  //
  template <bool v>
  static inline typename enable_if<v>::type
  cpfile (const path& from, const path& to,
          cpflags fl,
          permissions perm,
          auto_rmfile& rm)
  {
    ifdstream ifs (from, fdopen_mode::binary);

    fdopen_mode om (fdopen_mode::out      |
                    fdopen_mode::truncate |
                    fdopen_mode::create   |
                    fdopen_mode::binary);

    if ((fl & cpflags::overwrite_content) != cpflags::overwrite_content)
      om |= fdopen_mode::exclusive;

    ofdstream ofs (fdopen (to, om, perm));

    rm = auto_rmfile (to);

    // Throws ios::failure on fdstreambuf read/write failures.
    //
    // Note that the eof check is important: if the stream is at eof (empty
    // file) then this write will fail.
    //
    if (ifs.peek () != ifdstream::traits_type::eof ())
      ofs << ifs.rdbuf ();

    ifs.close (); // Throws ios::failure on failure.
    ofs.close (); // Throws ios::failure on flush/close failure.
  }

  template <bool v>
  static inline typename enable_if<!v>::type
  cpfile (const path& from, const path& to,
          cpflags fl,
          permissions perm,
          auto_rmfile& rm)
  {
    try
    {
      cpfile<true> (from, to, fl, perm, rm);
    }
    catch (const ios_base::failure& e)
    {
      // While we try to preserve the original error information, we can not
      // make the description to be exactly the same, for example
      //
      // Is a directory
      //
      // becomes
      //
      // Is a directory: Input/output error
      //
      // Note that our custom operator<<(ostream, exception) doesn't strip this
      // suffix. This is a temporary code after all.
      //
      throw_generic_error (EIO, e.what ());
    }
  }

  void
  cpfile (const path& from,
          const path& to,
          cpflags fl,
          optional<permissions> cperm)
  {
    permissions perm (cperm ? *cperm : path_permissions (from));
    auto_rmfile rm;

    cpfile<is_base_of<system_error, ios_base::failure>::value> (
      from, to, fl, perm, rm);

    if ((fl & cpflags::overwrite_permissions) ==
        cpflags::overwrite_permissions)
      path_permissions (to, perm);

    if ((fl & cpflags::copy_timestamps) == cpflags::copy_timestamps)
      file_time (to, file_time (from));

    rm.cancel ();
  }

  void
  mventry (const path& from, const path& to, cpflags fl)
  {
    assert ((fl & cpflags::overwrite_permissions) ==
            cpflags::overwrite_permissions);

    bool ovr ((fl & cpflags::overwrite_content) == cpflags::overwrite_content);

    const char* f (from.string ().c_str ());
    const char* t (to.string ().c_str ());

#ifndef _WIN32

    if (!ovr && path_entry (to).first)
      throw_generic_error (EEXIST);

    if (::rename (f, t) == 0) // POSIX implementation.
      return;

    // If source and destination paths are on different file systems we need to
    // move the file ourselves.
    //
    if (errno != EXDEV)
      throw_generic_error (errno);

    // Note that cpfile() follows symlinks, so we need to remove destination if
    // exists.
    //
    try_rmfile (to);

    // Note that permissions are copied unconditionally to a new file.
    //
    cpfile (from, to, cpflags::none);

    // Copy file access and modification times.
    //
    file_time (t, file_time (f));

    // Finally, remove the source file.
    //
    try_rmfile (from);

#else

    // While ::rename() is present on Windows, it is not POSIX but ISO C
    // implementation, that doesn't fit our needs well.
    //
    auto te (path_entry (to));

    if (!ovr && te.first)
      throw_generic_error (EEXIST);

    bool td (te.first && te.second.type == entry_type::directory);

    auto fe (path_entry (from));
    bool fd (fe.first && fe.second.type == entry_type::directory);

    // If source and destination filesystem entries exist, they both must be
    // either directories or not directories.
    //
    if (fe.first && te.first && fd != td)
      throw_generic_error (ENOTDIR);

    DWORD mfl (fd ? 0 : (MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING));

    // For reasons unknown an attempt to move a file sometimes ends up with
    // the 'file is being used by another process' error. If that's the case,
    // we will keep trying to move the file for two second.
    //
    // The thinking is that there can be some Windows process analyzing newly
    // created files and so preventing their move, removal, or change.
    //
    // Note that we address this issue in a similar way in try_rmfile() and
    // fdopen().
    //
    DWORD ec;
    for (size_t i (0); i < 41; ++i)
    {
      // Sleep 100 milliseconds before the move retry.
      //
      if (i != 0)
        Sleep (50);

      if (MoveFileExA (f, t, mfl))
        return;

      ec = GetLastError ();

      // If the destination already exists, then MoveFileExA() succeeds only
      // if it is a regular file or a file-type reparse point, failing with
      // ERROR_ALREADY_EXISTS (ERROR_ACCESS_DENIED under Wine) for a regular
      // directory and ERROR_ACCESS_DENIED for a directory-type reparse point.
      // Thus, we remove a directory-type reparse point and retry the move
      // operation.
      //
      // Lets also support an empty directory special case to comply with
      // POSIX. If the destination is an empty directory we will just remove
      // it and retry the move operation.
      //
      if (ec == ERROR_ALREADY_EXISTS || ec == ERROR_ACCESS_DENIED)
      {
        bool retry (false);

        if (te.first && directory_reparse_point (t))
        {
          // Note that the entry being removed is of the entry_type::symlink
          // or entry_type::other type.
          //
          try_rmfile (to);
          retry = true;
        }
        else if (td)
          retry = try_rmdir (path_cast<dir_path> (to)) !=
                  rmdir_status::not_empty;

        if (retry)
        {
          if (MoveFileExA (f, t, mfl))
            return;

          ec = GetLastError ();
        }
      }

      if (ec != ERROR_SHARING_VIOLATION)
        break;
    }

    throw_system_error (ec);

#endif
  }

  entry_time
  file_time (const char* p)
  {
    return entry_tm (p, false);
  }

  entry_time
  dir_time (const char* p)
  {
    return entry_tm (p, true);
  }

  void
  file_time (const char* p, const entry_time& t)
  {
    entry_tm (p, t, false);
  }

  void
  dir_time (const char* p, const entry_time& t)
  {
    entry_tm (p, t, true);
  }

  // dir_{entry,iterator}
  //
#ifndef _WIN32

  // dir_entry
  //
  dir_iterator::
  ~dir_iterator ()
  {
    if (h_ != nullptr)
      closedir (h_); // Ignore any errors.
  }

  dir_iterator& dir_iterator::
  operator= (dir_iterator&& x)
  {
    if (this != &x)
    {
      e_ = move (x.e_);

      if (h_ != nullptr && closedir (h_) == -1)
        throw_generic_error (errno);

      h_ = x.h_;
      x.h_ = nullptr;

      mode_ = x.mode_;
    }
    return *this;
  }

  static inline entry_type
  type (const struct stat& s) noexcept
  {
    if (S_ISREG (s.st_mode))
      return entry_type::regular;
    else if (S_ISDIR (s.st_mode))
      return entry_type::directory;
    else if (S_ISLNK (s.st_mode))
      return entry_type::symlink;
    else
      return entry_type::other;
  }

  entry_type dir_entry::
  type (bool follow_symlinks) const
  {
    // Note that this function can only be used for resolving an entry type
    // lazily and thus can't be used with the detect_dangling dir_iterator
    // mode (see dir_iterator::next () implementation for details). Thus, we
    // always throw on the stat()/lstat() failure.
    //
    path_type p (b_ / p_);
    struct stat s;
    if ((follow_symlinks
         ? stat (p.string ().c_str (), &s)
         : lstat (p.string ().c_str (), &s)) != 0)
      throw_generic_error (errno);

    entry_type r (butl::type (s));

    // While at it, also save the entry modification and access times.
    //
    if (r != entry_type::symlink)
    {
      entry_time t (entry_tm (s));
      mtime_ = t.modification;
      atime_ = t.access;
    }

    return r;
  }

  // dir_iterator
  //
  struct dir_deleter
  {
    void operator() (DIR* p) const {if (p != nullptr) closedir (p);}
  };

  dir_iterator::
  dir_iterator (const dir_path& d, mode m)
    : mode_ (m)
  {
    unique_ptr<DIR, dir_deleter> h (opendir (d.string ().c_str ()));
    h_ = h.get ();

    if (h_ == nullptr)
      throw_generic_error (errno);

    e_.b_ = d; // Used by next() to detect dangling symlinks.

    next ();

    h.release ();
  }

  template <typename D>
  static inline /*constexpr*/ optional<entry_type>
  d_type (const D* d, decltype(d->d_type)*)
  {
    switch (d->d_type)
    {
#ifdef DT_DIR
    case DT_DIR: return entry_type::directory;
#endif
#ifdef DT_REG
    case DT_REG: return entry_type::regular;
#endif
#ifdef DT_LNK
    case DT_LNK: return entry_type::symlink;
#endif
#ifdef DT_BLK
    case DT_BLK:
#endif
#ifdef DT_CHR
    case DT_CHR:
#endif
#ifdef DT_FIFO
    case DT_FIFO:
#endif
#ifdef DT_SOCK
    case DT_SOCK:
#endif
      return entry_type::other;

    default: return nullopt;
    }
  }

  template <typename D>
  static inline constexpr optional<entry_type>
  d_type (...) {return nullopt;}

  void dir_iterator::
  next ()
  {
    for (;;)
    {
      errno = 0;
      if (struct dirent* de = readdir (h_))
      {
        // We can accept some overhead for '.' and '..' (relying on short
        // string optimization) in favor of a more compact code.
        //
        path p (de->d_name);

        // Skip '.' and '..'.
        //
        if (p.current () || p.parent ())
          continue;

        e_.p_ = move (p);
        e_.t_ = d_type<struct dirent> (de, nullptr);
        e_.lt_ = nullopt;

        e_.mtime_ = timestamp_unknown;
        e_.atime_ = timestamp_unknown;

        // If requested, we ignore dangling symlinks, skipping ones with
        // non-existing or inaccessible targets (ignore_dangling mode), or set
        // the entry_type::unknown type for them (detect_dangling mode).
        //
        if (mode_ != no_follow)
        {
          bool dd (mode_ == detect_dangling);

          // Note that ltype () can potentially lstat() (see type() for
          // details) and so throw. We, however, need to skip the entry if it
          // is already removed (due to a race) and throw on any other error.
          //
          path fp (e_.base () / e_.path ());
          const char* p (fp.string ().c_str ());

          if (!e_.t_)
          {
            struct stat s;
            if (lstat (p, &s) != 0)
            {
              // Given that we have already enumerated the filesystem entry,
              // these error codes can only mean that the entry doesn't exist
              // anymore and so we always skip it.
              //
              // If errno is EACCES, then the permission to search a directory
              // we currently iterate over has been revoked. Throwing in this
              // case sounds like the best choice.
              //
              // Note that according to POSIX the filesystem entry we call
              // lstat() on doesn't require any specific permissions to be
              // granted.
              //
              if (errno == ENOENT || errno == ENOTDIR)
                continue;

              throw_generic_error (errno);
            }

            e_.t_ = type (s);

            if (*e_.t_ != entry_type::symlink)
            {
              entry_time t (entry_tm (s));
              e_.mtime_ = t.modification;
              e_.atime_ = t.access;
            }
          }

          // The entry type should be present and may not be
          // entry_type::unknown.
          //
          //assert (e_.t_ && *e_.t_ != entry_type::unknown);

          // Check if the symlink target exists and is accessible and set the
          // target type.
          //
          if (*e_.t_ == entry_type::symlink)
          {
            struct stat s;
            if (stat (p, &s) != 0)
            {
              if (errno == ENOENT || errno == ENOTDIR || errno == EACCES)
              {
                if (dd)
                  e_.lt_ = entry_type::unknown;
                else
                  continue;
              }
              else
                throw_generic_error (errno);
            }
            else
            {
              e_.lt_ = type (s);

              entry_time t (entry_tm (s));
              e_.mtime_ = t.modification;
              e_.atime_ = t.access;
            }
          }

          // The symlink target type should be present and in the
          // ignore_dangling mode it may not be entry_type::unknown.
          //
          //assert (*e_.t_ != entry_type::symlink ||
          //        (e_.lt_ && (dd || *e_.lt_ != entry_type::unknown)));
        }
      }
      else if (errno == 0)
      {
        // End of stream.
        //
        closedir (h_);
        h_ = nullptr;
      }
      else
        throw_generic_error (errno);

      break;
    }
  }

#else

  // dir_entry
  //
  entry_type dir_entry::
  type (bool follow_symlinks) const
  {
    // Note that this function can only be used for resolving an entry type
    // lazily and thus can't be used with the detect_dangling dir_iterator
    // mode (see dir_iterator::next () implementation for details). Thus, we
    // always throw if the entry info can't be retrieved.
    //
    // While at it, also save the entry modification and access times.
    //
    path_type p (base () / path ());
    entry_time et;
    pair<bool, entry_stat> e (
      path_entry (p, follow_symlinks, false /* ignore_error */, &et));

    if (!e.first)
      throw_generic_error (ENOENT);

    if (e.second.type == entry_type::regular ||
        e.second.type == entry_type::directory)
    {
      mtime_ = et.modification;
      atime_ = et.access;
    }

    return e.second.type;
  }

  // dir_iterator
  //
  static_assert(is_same<HANDLE, void*>::value, "HANDLE is not void*");

  static inline HANDLE
  to_handle (intptr_t h)
  {
    return reinterpret_cast<HANDLE> (h);
  }

  dir_iterator::
  ~dir_iterator ()
  {
    if (h_ != -1)
      FindClose (to_handle (h_)); // Ignore any errors.
  }

  dir_iterator& dir_iterator::
  operator= (dir_iterator&& x)
  {
    if (this != &x)
    {
      e_ = move (x.e_);

      if (h_ != -1 && !FindClose (to_handle (h_)))
        throw_system_error (GetLastError ());

      h_ = x.h_;
      x.h_ = -1;

      mode_ = x.mode_;
    }
    return *this;
  }

  dir_iterator::
  dir_iterator (const dir_path& d, mode m)
    : mode_ (m)
  {
    struct deleter
    {
      void operator() (intptr_t* p) const
      {
        if (p != nullptr && *p != -1)
          FindClose (to_handle (*p));
      }
    };

    unique_ptr<intptr_t, deleter> h (&h_);

    e_.b_ = d; // Used by next().

    next ();
    h.release ();
  }

  void dir_iterator::
  next ()
  {
    for (;;)
    {
      bool r;
      WIN32_FIND_DATA fi;

      if (h_ == -1)
      {
        // The call is made from the constructor. Any other call with h_ == -1
        // is illegal.
        //
        // Note that we used to check for the directory existence before
        // iterating over it. However, let's not pessimize things and only
        // check for the directory existence if FindFirstFileExA() fails.
        //

        h_ = reinterpret_cast<intptr_t> (
          FindFirstFileExA ((e_.base () / path ("*")).string ().c_str (),
                            FindExInfoBasic,
                            &fi,
                            FindExSearchNameMatch,
                            NULL,
                            0));

        r = (h_ != -1);
      }
      else
        r = FindNextFileA (to_handle (h_), &fi);

      if (r)
      {
        // We can accept some overhead for '.' and '..' (relying on short
        // string optimization) in favor of a more compact code.
        //
        path p (fi.cFileName);

        // Skip '.' and '..'.
        //
        if (p.current () || p.parent ())
          continue;

        e_.p_ = move (p);

        DWORD a (fi.dwFileAttributes);
        bool rp (reparse_point (a));

        // Evaluate the entry type lazily if this is a reparse point since it
        // requires to additionally query the entry information (see
        // reparse_point_entry() for details).
        //
        e_.t_ = rp            ? nullopt                                      :
                directory (a) ? optional<entry_type> (entry_type::directory) :
                                optional<entry_type> (entry_type::regular)   ;

        e_.lt_ = nullopt;

        e_.mtime_ = rp ? timestamp_unknown : to_timestamp (fi.ftLastWriteTime);

        // Note that according to MSDN for the FindFirstFile[Ex]() function
        // "the NTFS file system delays updates to the last access time for a
        // file by up to 1 hour after the last access" and "on the FAT file
        // system access time has a resolution of 1 day".
        //
        e_.atime_ = timestamp_unknown;

        // If requested, we ignore dangling symlinks and junctions, skipping
        // ones with non-existing or inaccessible targets (ignore_dangling
        // mode), or set the entry_type::unknown type for them
        // (detect_dangling mode).
        //
        if (rp && mode_ != no_follow)
        {
          bool dd (mode_ == detect_dangling);

          // Check the last error code throwing for codes other than "path not
          // found" and "access denied" and returning this error code
          // otherwise.
          //
          auto verify_error = [] ()
          {
            DWORD ec (GetLastError ());
            if (!error_file_not_found (ec) && ec != ERROR_ACCESS_DENIED)
              throw_system_error (ec);
            return ec;
          };

          // Note that ltype() queries the entry information due to the type
          // lazy evaluation (see above) and so can throw. We, however, need
          // to skip the entry if it is already removed (due to a race) and
          // throw on any other error.
          //
          path fp (e_.base () / e_.path ());
          const char* p (fp.string ().c_str ());

          pair<entry_type, path> rpe (
            reparse_point_entry (p, true /* ignore_error */));

          if (rpe.first == entry_type::unknown)
          {
            DWORD ec (verify_error ());

            // Silently skip the entry if it is not found (being already
            // deleted) or we are in the ignore dangling mode. Otherwise, set
            // the entry type to unknown.
            //
            // Note that sometimes trying to obtain information for a being
            // removed filesystem entry ends up with ERROR_ACCESS_DENIED (see
            // DeleteFile() and CreateFile() for details). Probably getting
            // this error code while trying to obtain the reparse point
            // information (involves calling CreateFile(FILE_READ_EA) and
            // DeviceIoControl()) can also be interpreted differently. We,
            // however, always treat it as "access denied" in the detect
            // dangling mode for good measure. Let's see if that won't be too
            // noisy.
            //
            if (ec != ERROR_ACCESS_DENIED || !dd)
              continue;

            // Fall through.
          }

          e_.t_ = rpe.first;

          // In this mode the entry type should be present and in the
          // ignore_dangling mode it may not be entry_type::unknown.
          //
          //assert (e_.t_ && (dd || *e_.t_ != entry_type::unknown));

          // Check if the symlink target exists and is accessible and set the
          // target type.
          //
          if (*e_.t_ == entry_type::symlink)
          {
            // Query the target info.
            //
            // Note that we use entry_info_handle() rather than
            // path_entry_handle_info() to be able to verify an error on
            // failure.
            //
            pair<auto_handle, BY_HANDLE_FILE_INFORMATION> ti (
              entry_info_handle (p,
                                 false /* write */,
                                 true /* follow_reparse_points */,
                                 true /* ignore_error */));

            if (ti.first == nullhandle)
            {
              verify_error ();

              if (dd)
                e_.lt_ = entry_type::unknown;
              else
                continue;
            }
            else
            {
              ti.first.close (); // Checks for error.

              e_.lt_ = directory (ti.second.dwFileAttributes)
                       ? entry_type::directory
                       : entry_type::regular;

              e_.mtime_ = to_timestamp (ti.second.ftLastWriteTime);
              e_.atime_ = to_timestamp (ti.second.ftLastAccessTime);
            }
          }

          // In this mode the symlink target type should be present and in the
          // ignore_dangling mode it may not be entry_type::unknown.
          //
          //assert (*e_.t_ != entry_type::symlink ||
          //        (e_.lt_ && (dd || *e_.lt_ != entry_type::unknown)));
        }
      }
      else
      {
        DWORD ec (GetLastError ());
        bool first (h_ == -1);

        // Check to distinguish non-existent vs empty directories.
        //
        // Note that dir_exists() handles not only the "filesystem entry does
        // not exist" case but also the case when the entry exists but is not
        // a directory.
        //
        if (first && !dir_exists (e_.base ()))
          throw_generic_error (ENOENT);

        if (ec == (first ? ERROR_FILE_NOT_FOUND : ERROR_NO_MORE_FILES))
        {
          // End of stream.
          //
          if (h_ != -1)
          {
            FindClose (to_handle (h_));
            h_ = -1;
          }
        }
        else
          throw_system_error (ec);
      }

      break;
    }
  }
#endif

  // Search for paths matching the pattern and call the specified function for
  // each matching path. Return false if the underlying func() or
  // dangling_func() call returns false. Otherwise the function conforms to
  // the path_search() description.
  //
  // Note that the access to the traversed directory tree (real or virtual) is
  // performed through the provided filesystem object.
  //
  static const string any_dir ("*/");

  // Filesystem traversal callbacks.
  //
  // Called before entering a directory for the recursive traversal. If
  // returns false, then the directory is not entered.
  //
  using preopen = function<bool (const dir_path&)>;

  // Called before skipping a dangling link. If returns false, then the
  // traversal is stopped.
  //
  using preskip = function<bool (const dir_entry&)>;

  template <typename FS>
  static bool
  search (
    path pattern,
    dir_path pattern_dir,
    path_match_flags fl,
    const function<bool (path&&, const string& pattern, bool interm)>& func,
    const function<bool (const dir_entry&)>& dangling_func,
    FS& filesystem)
  {
    bool follow_symlinks ((fl & path_match_flags::follow_symlinks) !=
                          path_match_flags::none);

    assert (follow_symlinks || dangling_func == nullptr);

    // Fast-forward the leftmost pattern non-wildcard components. So, for
    // example, search for foo/f* in /bar/ becomes search for f* in /bar/foo/.
    //
    {
      auto b (pattern.begin ());
      auto e (pattern.end ());
      auto i (b);
      for (; i != e && !path_pattern (*i); ++i) ;

      // If the pattern has no wildcards then we reduce to checking for the
      // filesystem entry existence. It matches if exists and is of the proper
      // type.
      //
      if (i == e)
      {
        path p (pattern_dir / pattern);
        auto pe (filesystem.path_entry (p, follow_symlinks));

        if (pe.first &&
            ((pe.second.type == entry_type::directory) == p.to_directory ()))
          return func (move (p), string (), false);

        return true;
      }
      else if (i != b) // There are non-wildcard components, so fast-forward.
      {
        path p (b, i);
        pattern = pattern.leaf (p);
        pattern_dir /= path_cast<dir_path> (move (p));
      }
    }

    assert (!pattern.empty ());

    // The pattern leftmost component. Will use it to match the start directory
    // sub-entries.
    //
    path pc (pattern.begin (), ++pattern.begin ());
    string pcr (pc.representation ());

    // Note that if the pattern has multiple components (is not a simple path),
    // then the leftmost one has a trailing separator, and so will match
    // sub-directories only.
    //
    bool simple (pattern.simple ());

    // If symlinks need to be followed, then pass the preskip callback for the
    // filesystem iterator.
    //
    bool fs (follow_symlinks || !simple);
    preskip ps;
    bool dangling_stop (false);

    if (fs)
    {
      if (dangling_func != nullptr)
      {
        // Note that we rely on the "small function object" optimization here.
        //
        ps = [&dangling_func, &dangling_stop] (const dir_entry& de) -> bool
        {
          dangling_stop = !dangling_func (de);
          return !dangling_stop;
        };
      }
      else
      {
        ps = [] (const dir_entry& de) -> bool
        {
          throw_generic_error (
            de.ltype () == entry_type::symlink ? ENOENT : EACCES);
        };
      }
    }

    // Note that we rely on the "small function object" optimization here.
    //
    typename FS::iterator_type i (filesystem.iterator (
      pattern_dir,
      path_pattern_recursive (pcr),
      path_pattern_self_matching (pcr),
      fs,
      [&pattern_dir, &func] (const dir_path& p) -> bool // Preopen.
      {
        return func (pattern_dir / p, any_dir, true);
      },
      move (ps)));

    // Canonicalize the pattern component collapsing consecutive stars (used to
    // express that it is recursive) into a single one.
    //
    auto j (pcr.begin ());
    bool prev_star (false);
    for (const path_pattern_term& t: path_pattern_iterator (pcr))
    {
      // Skip the repeated star wildcard.
      //
      if (t.star () && prev_star)
        continue;

      // Note: we only need to copy the pattern term if a star have already
      // been skipped.
      //
      assert (j <= t.begin);

      if (j != t.begin)
        copy (t.begin, t.end, j);

      j += t.size ();

      prev_star = t.star ();
    }

    if (j != pcr.end ())
      pcr.resize (j - pcr.begin ());

    // Note that the callback function can be called for the same directory
    // twice: first time as intermediate match from iterator's preopen() call,
    // and then, if the first call succeed, from the iterating loop (possibly
    // as the final match).
    //
    path p;
    while (i.next (p))
    {
      // Skip sub-entry if its name doesn't match the pattern leftmost
      // component.
      //
      // Matching the directory we are iterating through (as for a pattern
      // component containing ***) is a bit tricky. This directory is
      // represented by the iterator as an empty path, and so we need to
      // compute it (the leaf would actually be enough) for matching. This
      // leaf can be acquired from the pattern_dir (if not empty) or
      // start_dir. We don't expect the start_dir to be empty, as the
      // filesystem object must replace an empty start directory with the
      // current one. This is the case when we search in the current directory
      // (start_dir is empty) with a pattern that starts with a *** wildcard
      // (for example f***/bar).  Note that this will be the only case per
      // path_search() as the next time pattern_dir will not be empty. Also
      // note that this is never the case for a pattern that is an absolute
      // path, as the first component cannot be a wildcard (is empty for POSIX
      // and is drive for Windows).
      //
      const path& se (!p.empty ()
                      ? p
                      : path_cast<path> (!pattern_dir.empty ()
                                         ? pattern_dir
                                         : filesystem.start_dir ()));

      if (!path_match (se.leaf ().representation (), pcr))
        continue;

      // If the callback function returns false, then we stop the entire search
      // for the final match, or do not search below the path for the
      // intermediate one.
      //
      if (!func (pattern_dir / p, pcr, !simple))
      {
        if (simple) // Final match.
          return false;
        else
          continue;
      }

      // If the pattern is not a simple one, and it's leftmost component
      // matches the sub-entry, then the sub-entry is a directory (see the note
      // above), and we search in it using the trailing part of the pattern.
      //
      if (!simple && !search (pattern.leaf (pc),
                              pattern_dir / path_cast<dir_path> (move (p)),
                              fl,
                              func,
                              dangling_func,
                              filesystem))
        return false;
    }

    if (dangling_stop)
      return false;

    // If requested, also search with the absent-matching pattern path
    // component omitted, unless this is the only pattern component.
    //
    if ((fl & path_match_flags::match_absent) != path_match_flags::none &&
        pc.to_directory ()                                              &&
        (!pattern_dir.empty () || !simple)                              &&
        pc.string ().find_first_not_of ('*') == string::npos            &&
        !search (pattern.leaf (pc),
                 pattern_dir,
                 fl,
                 func,
                 dangling_func,
                 filesystem))
    {
      return false;
    }

    return true;
  }

  // Path search implementations.
  //
  static const dir_path empty_dir;

  // Base for filesystem (see above) implementations.
  //
  // Don't copy start directory. It is expected to exist till the end of the
  // object lifetime.
  //
  class filesystem_base
  {
  protected:
    filesystem_base (const dir_path& start): start_ (start) {}

  public:
    const dir_path&
    start_dir ()
    {
      if (!start_.empty ())
        return start_;

      if (current_.empty ())
        current_ = dir_path::current_directory ();

      return current_;
    }

  protected:
    const dir_path& start_;
    dir_path current_;
  };

  // Search path in the real filesystem.
  //
  // Iterate over directory sub-entries, recursively and including itself if
  // requested. Note that recursive iterating goes depth-first which make
  // sense for the cleanup use cases (@@ maybe this should be controllable
  // since for directory creation it won't make sense).
  //
  // Prior to recursively opening a directory for iterating the preopen
  // callback function is called. If false is returned, then the directory is
  // not traversed but still returned by the next() call.
  //
  // Note that iterating over non-existent directory is not en error. The
  // subsequent next() call returns false for such a directory.
  //
  class recursive_dir_iterator
  {
  public:
    recursive_dir_iterator (dir_path p,
                            bool recursive,
                            bool self,
                            bool fs,
                            preopen po,
                            preskip ps)
        : start_ (move (p)),
          recursive_ (recursive),
          self_ (self),
          follow_symlinks_ (fs),
          preopen_ (move (po)),
          preskip_ (move (ps))
    {
      assert (fs || ps == nullptr);

      open (dir_path (), self_);
    }

    // Move constructible-only, non-assignable type.
    //
    recursive_dir_iterator (const recursive_dir_iterator&) = delete;
    recursive_dir_iterator& operator= (const recursive_dir_iterator&) = delete;
    recursive_dir_iterator (recursive_dir_iterator&&) = default;

    // Return false if no more entries left. Otherwise save the next entry
    // path and return true. The path is relative to the directory being
    // traversed and contains a trailing separator for sub-directories. Throw
    // std::system_error in case of a failure (insufficient permissions,
    // dangling symlink encountered, etc).
    //
    // If symlinks need to be followed, then skip inaccessible/dangling
    // entries or, if the preskip callback is specified and returns false for
    // such an entry, stop the entire traversal.
    //
    bool
    next (path& p)
    {
      if (iters_.empty ())
        return false;

      auto& i (iters_.back ());

      for (;;) // Skip inaccessible/dangling entries.
      {
        // If we got to the end of directory sub-entries, then go one level up
        // and return this directory path.
        //
        if (i.first == dir_iterator ())
        {
          path d (move (i.second));
          iters_.pop_back ();

          // Return the path unless it is the last one (the directory we
          // started to iterate from) and the self flag is not set.
          //
          if (iters_.empty () && !self_)
            return false;

          p = move (d);
          return true;
        }

        const dir_entry& de (*i.first);

        // Append separator if a directory. Note that dir_entry::type() can
        // throw.
        //
        entry_type et (follow_symlinks_ ? de.type () : de.ltype ());

        // If the entry turned out to be inaccessible/dangling, then skip it
        // if the preskip function is not specified or returns true and stop
        // the entire traversal otherwise.
        //
        if (et == entry_type::unknown)
        {
          if (preskip_ != nullptr && !preskip_ (de))
          {
            iters_.clear ();
            return false;
          }

          ++i.first;
          continue;
        }

        path pe (et == entry_type::directory
                 ? path_cast<dir_path> (i.second / de.path ())
                 : i.second / de.path ());

        ++i.first;

        if (recursive_ && pe.to_directory ())
        {
          open (path_cast<dir_path> (move (pe)), true);
          return next (p);
        }

        p = move (pe);
        return true;
      }
    }

  private:
    void
    open (dir_path p, bool preopen)
    {
      // We should consider a racing condition here. The directory can be
      // removed before we create an iterator for it. In this case we just do
      // nothing, so the directory is silently skipped.
      //
      try
      {
        // If preopen_() returns false, then the directory will not be
        // traversed (since we leave iterator with end semantics) but still be
        // returned by the next() call as a sub-entry.
        //
        dir_iterator i;
        if (!preopen || preopen_ (p))
        {
          dir_path d (start_ / p);

          // If we follow symlinks, then we may need to skip the dangling
          // ones. Note, however, that we will be skipping them not at the
          // dir_iterator level but ourselves, after calling the preskip
          // callback function (see next() for details).
          //
          i = dir_iterator (!d.empty () ? d : dir_path ("."),
                            follow_symlinks_
                            ? dir_iterator::detect_dangling
                            : dir_iterator::no_follow);
        }

        iters_.emplace_back (move (i), move (p));
      }
      catch (const system_error& e)
      {
        // Ignore non-existent directory (ENOENT or ENOTDIR). Rethrow for any
        // other error. We consider ENOTDIR as a variety of removal, with a
        // new filesystem entry being created afterwards.
        //
        // Make sure that the error denotes errno portable code.
        //
        if (e.code ().category () == generic_category ())
        {
          int ec (e.code ().value ());
          if (ec == ENOENT || ec == ENOTDIR)
            return;
        }

        throw;
      }
    }

  private:
    dir_path start_;
    bool recursive_;
    bool self_;
    bool follow_symlinks_;
    preopen preopen_;
    preskip preskip_;
    small_vector<pair<dir_iterator, dir_path>, 1> iters_;
  };

  // Provide an access to the real filesystem.
  //
  class real_filesystem: public filesystem_base
  {
  public:
    using iterator_type = recursive_dir_iterator;

    real_filesystem (const dir_path& start): filesystem_base (start) {}

    pair<bool, entry_stat>
    path_entry (const path& p, bool follow_symlinks) const
    {
      return butl::path_entry (start_ / p, follow_symlinks);
    }

    iterator_type
    iterator (const dir_path& p,
              bool recursive,
              bool self,
              bool follow_symlinks,
              preopen po,
              preskip ps) const
    {
      return iterator_type (start_ / p,
                            recursive,
                            self,
                            follow_symlinks,
                            move (po),
                            move (ps));
    }
  };

  void
  path_search (
    const path& pattern,
    const function<bool (path&&, const string& pattern, bool interm)>& func,
    const dir_path& start,
    path_match_flags flags,
    const function<bool (const dir_entry&)>& dangling_func)
  {
    real_filesystem fs (pattern.relative () ? start : empty_dir);
    search (pattern, dir_path (), flags, func, dangling_func, fs);
  }

  // Search path in the directory tree represented by a path.
  //
  // Iterate over path prefixes, as recursive_dir_iterator (see above) would
  // iterate over the real directory tree.
  //
  class path_iterator
  {
  public:
    path_iterator (path p, bool recursive, bool self, preopen po)
        : path_ (move (p)),
          recursive_ (recursive),
          self_ (self),
          preopen_ (move (po)),
          iter_ (path_.begin ())
    {
      open (dir_path (), self_);
    }

    // Behave as recursive_dir_iterator (see above) would do if iterating over
    // non-existent directory. Note that this behavior differs from that for
    // an empty directory (previous ctor being called with an empty path). If
    // self flag is true, then, for the empty directory, the first next() call
    // returns true and saves empty path (self sub-entry). For non-existent
    // directory the first next() call returns false. Also note that
    // recursive_dir_iterator calls the preopen function before it becomes
    // known that the directory doesn't exist, and we emulate such a behavior
    // here as well.
    //
    path_iterator (bool self, preopen po)
        : self_ (false),
          iter_ (path_.begin ())
    {
      if (self)
        po (empty_dir);
    }

    // Move constructible-only, non-assignable type.
    //
    path_iterator (const path_iterator&) = delete;
    path_iterator& operator= (const path_iterator&) = delete;

    // Custom move ctor (properly moves path/iterator pair).
    //
    path_iterator (path_iterator&& pi)
        : path_ (move (pi.path_)),
          recursive_ (pi.recursive_),
          self_ (pi.self_),
          preopen_ (move (pi.preopen_)),
          iter_ (path_, pi.iter_) {}

    // Return false if no more entries left. Otherwise save the next entry path
    // and return true.
    //
    bool
    next (path& p)
    {
      if (iter_ == path_.begin ())
      {
        if (!self_)
          return false;

        p = path ();
        self_ = false; // To bail out the next time.
        return true;
      }

      path pe (path_.begin (), iter_);
      if (recursive_ && pe.to_directory ())
      {
        open (path_cast<dir_path> (move (pe)), true);
        return next (p);
      }

      --iter_; // Return one level up.

      p = move (pe);
      return true;
    }

  private:
    void
    open (const dir_path& p, bool preopen)
    {
      // If preopen_() returns false, then the directory will not be
      // traversed (since we reset the recursive flag) but still be returned
      // by the next() call as a sub-entry.
      //
      if (preopen && !preopen_ (p))
        recursive_ = false;
      else if (iter_ != path_.end ())
        ++iter_;

      // If the rightmost component is reached, then all the directories were
      // traversed, so we reset the recursive flag.
      //
      if (iter_ == path_.end ())
        recursive_ = false;
    }

  private:
    path path_;
    bool recursive_;
    bool self_;
    preopen preopen_;
    path::iterator iter_;
  };

  // Provide an access to a directory tree, that is represented by the path.
  //
  // Note that symlinks are meaningless for this filesystem.
  //
  class path_filesystem: public filesystem_base
  {
  public:
    using iterator_type = path_iterator;

    path_filesystem (const dir_path& start, const path& p)
        : filesystem_base (start),
          path_ (p) {}

    pair<bool, entry_stat>
    path_entry (const path& p, bool /*follow_symlinks*/)
    {
      // If path and sub-path are non-empty, and both are absolute or relative,
      // then no extra effort is required (prior to checking if one is a
      // sub-path or the other). Otherwise we complete the relative paths
      // first.
      //
      auto path_entry = [] (const path& p, const path& pe)
      {
        // Note that paths are not required to be normalized, so we just check
        // that one path is a literal prefix of the other one.
        //
        if (!p.sub (pe))
          return make_pair (false, entry_stat {entry_type::unknown, 0});

        entry_type t (pe == p && !p.to_directory ()
                      ? entry_type::regular
                      : entry_type::directory);

        return make_pair (true, entry_stat {t, 0});
      };

      if (path_.relative () == p.relative () && !path_.empty () && !p.empty ())
        return path_entry (path_, p);

      return path_entry (path_.absolute () ? path_ : complete (path_),
                         p.absolute () ? p : complete (p));
    }

    iterator_type
    iterator (const dir_path& p,
              bool recursive,
              bool self,
              bool /*follow_symlinks*/,
              preopen po,
              preskip)
    {
      // If path and sub-path are non-empty, and both are absolute or relative,
      // then no extra effort is required (prior to checking if one is a
      // sub-path or the other). Otherwise we complete the relative paths
      // first.
      //
      auto iterator = [recursive, self, &po] (const path& p,
                                              const dir_path& pe)
      {
        // If the directory we should iterate belongs to the directory tree,
        // then return the corresponding leaf path iterator. Otherwise return
        // the non-existent directory iterator (returns false on the first
        // next() call).
        //
        return p.sub (pe)
          ? iterator_type (p.leaf (pe), recursive, self, move (po))
          : iterator_type (self, move (po));
      };

      if (path_.relative () == p.relative () && !path_.empty () && !p.empty ())
        return iterator (path_, p);

      return iterator (path_.absolute () ? path_ : complete (path_),
                       p.absolute () ? p : path_cast<dir_path> (complete (p)));
    }

  private:
    // Complete the relative path.
    //
    path
    complete (const path& p)
    {
      assert (p.relative ());

      if (start_.absolute ())
        return start_ / p;

      if (current_.empty ())
        current_ = dir_path::current_directory ();

      return !start_.empty ()
        ? current_ / start_ / p
        : current_ / p;
    }

  private:
    const path& path_;
  };

  void
  path_search (
    const path& pattern,
    const path& entry,
    const function<bool (path&&, const string& pattern, bool interm)>& func,
    const dir_path& start,
    path_match_flags flags)
  {
    path_filesystem fs (start, entry);
    search (pattern, dir_path (), flags, func, nullptr /* dangle_func */, fs);
  }
}
