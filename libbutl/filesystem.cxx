// file      : libbutl/filesystem.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules_ts
#include <libbutl/filesystem.mxx>
#endif

#include <errno.h> // errno, E*

#ifndef _WIN32
#  include <stdio.h>     // rename()
#  include <utime.h>     // utime()
#  include <dirent.h>    // struct dirent, *dir()
#  include <unistd.h>    // symlink(), link(), stat(), rmdir(), unlink()
#  include <sys/time.h>  // utimes()
#  include <sys/types.h> // stat
#  include <sys/stat.h>  // stat(), lstat(), S_I*, mkdir(), chmod()
#else
#  include <libbutl/win32-utility.hxx>

#  include <io.h>        // _find*(), _unlink(), _chmod()
#  include <direct.h>    // _mkdir(), _rmdir()
#  include <winioctl.h>  // FSCTL_SET_REPARSE_POINT
#  include <sys/types.h> // _stat
#  include <sys/stat.h>  // _stat(), S_I*
#  include <sys/utime.h> // _utime()

#  include <cwchar> // mbsrtowcs(), mbstate_t

#  ifdef _MSC_VER // Unlikely to be fixed in newer versions.
#    define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#    define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#  endif
#endif

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <string>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <iterator>
#include <functional>

#include <vector>
#include <memory>       // unique_ptr
#include <algorithm>    // find(), copy()
#include <system_error>
#endif

// Other includes.

#ifdef __cpp_modules_ts
module butl.filesystem;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules_ts
import std.core;
#endif
import butl.path;
import butl.timestamp;
import butl.path_pattern;
#endif

import butl.utility;      // throw_generic_error()
import butl.fdstream;
import butl.small_vector;
#else
#include <libbutl/path.mxx>
#include <libbutl/utility.mxx>
#include <libbutl/fdstream.mxx>
#include <libbutl/small-vector.mxx>
#endif

using namespace std;

namespace butl
{
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
    auto pe (path_entry (p, true, ie));
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
  junction (DWORD a) noexcept
  {
    return a != INVALID_FILE_ATTRIBUTES            &&
           (a & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
           (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
  }

  static inline bool
  junction (const path& p) noexcept
  {
    return junction (GetFileAttributesA (p.string ().c_str ()));
  }

  // Return true if the junction exists and is referencing an existing
  // directory. Assume that the path references a junction. Underlying OS
  // errors are reported by throwing std::system_error, unless ignore_error
  // is true.
  //
  static bool
  junction_target_exists (const char* p, bool ignore_error)
  {
    HANDLE h (CreateFile (p,
                          FILE_READ_ATTRIBUTES,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL,
                          OPEN_EXISTING,
                          FILE_FLAG_BACKUP_SEMANTICS,
                          NULL));

    if (h == INVALID_HANDLE_VALUE)
    {
      DWORD ec;

      if (ignore_error || error_file_not_found (ec = GetLastError ()))
        return false;

      throw_system_error (ec);
    }

    CloseHandle (h);
    return true;
  }

  static inline bool
  junction_target_exists (const path& p, bool ignore_error)
  {
    return junction_target_exists (p.string ().c_str (), ignore_error);
  }

  pair<bool, entry_stat>
  path_entry (const char* p, bool fl, bool ie)
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
      d = p;
      d += path::traits_type::directory_separator;
      p = d.c_str ();
    }

    // Note that VC's implementations of _stat64() follows junctions and fails
    // for dangling ones. MinGW GCC's implementation returns the information
    // about the junction itself. That's why we handle junctions specially,
    // not relying on _stat64().
    //
    DWORD a (GetFileAttributesA (p));
    if (a == INVALID_FILE_ATTRIBUTES) // Presumably not exists.
      return make_pair (false, entry_stat {entry_type::unknown, 0});

    if (junction (a))
    {
      if (!fl)
        return make_pair (true, entry_stat {entry_type::symlink, 0});

      return junction_target_exists (p, ie)
        ? make_pair (true,  entry_stat {entry_type::directory, 0})
        : make_pair (false, entry_stat {entry_type::unknown, 0});
    }

    entry_type et (entry_type::unknown);
    struct __stat64 s; // For 64-bit size.

    if (_stat64 (p, &s) != 0)
    {
      if (errno == ENOENT || errno == ENOTDIR || ie)
        return make_pair (false, entry_stat {et, 0});
      else
        throw_generic_error (errno);
    }

    // Note that we currently support only directory symlinks (see mksymlink()
    // for details).
    //
    if (S_ISREG (s.st_mode))
      et = entry_type::regular;
    else if (S_ISDIR (s.st_mode))
      et = entry_type::directory;
    //
    //else if (S_ISLNK (s.st_mode))
    //  et = entry_type::symlink;

    return make_pair (true,
                      entry_stat {et, static_cast<uint64_t> (s.st_size)});
  }

#endif

  bool
  touch_file (const path& p, bool create)
  {
    if (file_exists (p))
    {
      // Note that on Windows there are two times: the precise time (which is
      // what we get with system_clock::now()) and what we will call the
      // "filesystem time", which can lag the precise time by as much as a
      // couple of milliseconds. To get the filesystem time use
      // GetSystemTimeAsFileTime(). This is just a heads-up in case we decide
      // to change this code at some point.
      //
#ifndef _WIN32
      if (utime (p.string ().c_str (), nullptr) == -1)
#else
      if (_utime (p.string ().c_str (), nullptr) == -1)
#endif
        throw_generic_error (errno);

      return false;
    }

    if (create && !entry_exists (p))
    {
      // Assuming the file access and modification times are set to the
      // current time automatically.
      //
      fdopen (p, fdopen_mode::out | fdopen_mode::create);
      return true;
    }

    throw_generic_error (ENOENT); // Does not exist or not a file.
  }

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

#ifndef _WIN32
    if (rmdir (p.string ().c_str ()) != 0)
#else
    if (_rmdir (p.string ().c_str ()) != 0)
#endif
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
      for (const dir_entry& de: dir_iterator (p, false /* ignore_dangling */))
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

  rmfile_status
  try_rmfile (const path& p, bool ignore_error)
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
    // directory symlink.
    //
    // And also there are some unknown reasons for the 'permission denied'
    // failure (see mventry() for details). If that's the case, we will keep
    // trying to move the file for a second.
    //
    for (size_t i (0); i < 11; ++i)
    {
      // Sleep 100 milliseconds before the removal retry.
      //
      if (i != 0)
        Sleep (100);

      ur = _unlink (f);

      if (ur != 0 && errno == EACCES)
      {
        DWORD a (GetFileAttributesA (f));
        if (a != INVALID_FILE_ATTRIBUTES)
        {
          bool readonly ((a & FILE_ATTRIBUTE_READONLY) != 0);

          // Note that we support only directory symlinks on Windows.
          //
          bool symlink (junction (a));

          if (readonly || symlink)
          {
            bool restore (readonly &&
                          SetFileAttributes (f, a & ~FILE_ATTRIBUTE_READONLY));

            ur = symlink ? _rmdir (f) : _unlink (f);

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

  rmfile_status
  try_rmsymlink (const path& link, bool, bool ie)
  {
    return try_rmfile (link, ie);
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
    if (!dir)
      throw_generic_error (ENOSYS, "file symlinks not supported");

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

    HANDLE h (CreateFile (link.string ().c_str (),
                          GENERIC_WRITE,
                          0,
                          NULL,
                          OPEN_EXISTING,
                          FILE_FLAG_BACKUP_SEMANTICS |
                          FILE_FLAG_OPEN_REPARSE_POINT,
                          NULL));

    if (h == INVALID_HANDLE_VALUE)
      throw_system_error (GetLastError ());

    unique_ptr<HANDLE, void (*)(HANDLE*)> deleter (
      &h,
      [] (HANDLE* h)
      {
        bool r (CloseHandle (*h));

        // The valid file handle that has no IO operations being performed on
        // it should close successfully, unless something is severely damaged.
        //
        assert (r);
      });

    // We will fill the uninitialized members later.
    //
    struct
    {
      // Header.
      //
      DWORD   reparse_tag            = IO_REPARSE_TAG_MOUNT_POINT;
      WORD    reparse_data_length;
      WORD    reserved               = 0;

      // Mount point reparse buffer.
      //
      WORD    substitute_name_offset = 0;
      WORD    substitute_name_length;
      WORD    print_name_offset;
      WORD    print_name_length      = 0;

      // Reserve space for two NULL characters (for the names above).
      //
      wchar_t path_buffer[MAX_PATH + 2];
    } rb;

    // Make the target path absolute, decorate it and convert to a
    // wide-character string.
    //
    path atd (target);

    if (atd.relative ())
      atd.complete ();

    try
    {
      atd.normalize ();
    }
    catch (const invalid_path&)
    {
      throw_generic_error (EINVAL);
    }

    string td ("\\??\\" + atd.string () + "\\");
    const char* s (td.c_str ());

    // Zero-initialize the conversion state (and disambiguate with the
    // function declaration).
    //
    mbstate_t state ((mbstate_t ()));
    size_t n (mbsrtowcs (rb.path_buffer, &s, MAX_PATH + 1, &state));

    if (n == static_cast<size_t> (-1))
      throw_generic_error (errno);

    if (s != NULL) // Not enough space in the destination buffer.
      throw_generic_error (ENAMETOOLONG);

    // Fill the rest of the structure and setup the reparse point.
    //
    // The path length not including NULL character, in bytes.
    //
    WORD nb (static_cast<WORD> (n) * sizeof (wchar_t));
    rb.substitute_name_length = nb;

    // The print name offset, in bytes.
    //
    rb.print_name_offset  = nb + sizeof (wchar_t);
    rb.path_buffer[n + 1] = L'\0'; // Terminate the (empty) print name.

    // The mount point reparse buffer size.
    //
    rb.reparse_data_length =
      4 * sizeof (WORD)     + // Size of *_name_* fields.
      nb + sizeof (wchar_t) + // Path length, in bytes.
      sizeof (wchar_t);       // Print (empty) name length, in bytes.

    DWORD r;
    if (!DeviceIoControl (
          h,
          FSCTL_SET_REPARSE_POINT,
          &rb,
          sizeof (DWORD) + 2 * sizeof (WORD) + // Size of the header.
          rb.reparse_data_length,              // Size of the mount point
          NULL,                                // reparse buffer.
          0,
          &r,
          NULL))
      throw_system_error (GetLastError ());

    rm.cancel ();
  }

  rmfile_status
  try_rmsymlink (const path& link, bool dir, bool ie)
  {
    if (!dir)
      throw_generic_error (ENOSYS, "file symlinks not supported");

    return try_rmfile (link, ie);
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

  entry_type
  mkanylink (const path& target, const path& link, bool copy, bool rel)
  {
    using error = pair<entry_type, system_error>;

    try
    {
      mksymlink (rel ? target.relative (link.directory ()) : target, link);
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
          try
          {
            mkhardlink (target, link);
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
                  cpfile (target, link);
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

    // Throws ios::failure on fdbuf read/write failures.
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
  cpfile (const path& from, const path& to, cpflags fl)
  {
    permissions perm (path_permissions (from));
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
    // we will keep trying to move the file for a second.
    //
    // The thinking is that there can be some Windows process analyzing newly
    // created files and so preventing their move or removal.
    //
    DWORD ec;
    for (size_t i (0); i < 11; ++i)
    {
      // Sleep 100 milliseconds before the move retry.
      //
      if (i != 0)
        Sleep (100);

      if (MoveFileExA (f, t, mfl))
        return;

      ec = GetLastError ();

      // If the destination already exists, then MoveFileExA() succeeds only
      // if it is a regular file or a symlink. Lets also support an empty
      // directory special case to comply with POSIX. If the destination is an
      // empty directory we will just remove it and retry the move operation.
      //
      // Note that under Wine we endup with ERROR_ACCESS_DENIED error code in
      // that case, and with ERROR_ALREADY_EXISTS when run natively.
      //
      if ((ec == ERROR_ALREADY_EXISTS || ec == ERROR_ACCESS_DENIED) && td &&
          try_rmdir (path_cast<dir_path> (to)) != rmdir_status::not_empty)
      {
        if (MoveFileExA (f, t, mfl))
          return;

        ec = GetLastError ();
      }

      if (ec != ERROR_SHARING_VIOLATION)
        break;
    }

    throw_system_error (ec);

#endif
  }

  // Return the modification and access times of a regular file or directory.
  //
  static entry_time
  entry_tm (const char* p, bool dir)
  {
#ifndef _WIN32

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

    auto tm = [] (time_t sec, auto nsec) -> timestamp
    {
      return system_clock::from_time_t (sec) +
        chrono::duration_cast<duration> (chrono::nanoseconds (nsec));
    };

    return {tm (s.st_mtime, mnsec<struct stat> (&s, true)),
            tm (s.st_atime, ansec<struct stat> (&s, true))};

#else

    WIN32_FILE_ATTRIBUTE_DATA s;

    if (!GetFileAttributesExA (p, GetFileExInfoStandard, &s))
    {
      DWORD ec (GetLastError ());

      if (error_file_not_found (ec))
        return {timestamp_nonexistent, timestamp_nonexistent};

      throw_system_error (ec);
    }

    if ((s.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) !=
        (dir ? FILE_ATTRIBUTE_DIRECTORY : 0))
      return {timestamp_nonexistent, timestamp_nonexistent};

    auto tm = [] (const FILETIME& t) -> timestamp
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
    };

    return {tm (s.ftLastWriteTime), tm (s.ftLastAccessTime)};

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

  // Set the modification and access times for a regular file or directory.
  //
  static void
  entry_tm (const char* p, const entry_time& t, bool dir)
  {
#ifndef _WIN32

    struct stat s;
    if (stat (p, &s) != 0)
      throw_generic_error (errno);

    // If the entry is of the wrong type, then let's pretend that it doesn't
    // exists. In other words, the entry of the required type doesn't exist.
    //
    if (dir ? !S_ISDIR (s.st_mode) : !S_ISREG (s.st_mode))
      return throw_generic_error (ENOENT);

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

#else

    DWORD attr (GetFileAttributesA (p));

    if (attr == INVALID_FILE_ATTRIBUTES)
      throw_system_error (GetLastError ());

    // If the entry is of the wrong type, then let's pretend that it doesn't
    // exists.
    //
    if ((attr & FILE_ATTRIBUTE_DIRECTORY) !=
        (dir ? FILE_ATTRIBUTE_DIRECTORY : 0))
      return throw_generic_error (ENOENT);

    HANDLE h (CreateFile (p,
                          FILE_WRITE_ATTRIBUTES,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL,
                          OPEN_EXISTING,
                          dir ? FILE_FLAG_BACKUP_SEMANTICS : 0,
                          NULL));

    if (h == INVALID_HANDLE_VALUE)
      throw_system_error (GetLastError ());

    unique_ptr<HANDLE, void (*)(HANDLE*)> deleter (
      &h, [] (HANDLE* h)
      {
        bool r (CloseHandle (*h));

        // The valid file handle that has no IO operations being performed on
        // it should close successfully, unless something is severely damaged.
        //
        assert (r);
      });

    auto tm = [] (timestamp t, FILETIME& ft) -> const FILETIME*
    {
      if (t == timestamp_nonexistent)
        return NULL;

      // Time in FILETIME is in 100 nanosecond "ticks" since "Windows epoch"
      // (1601-01-01T00:00:00Z). To convert "UNIX epoch"
      // (1970-01-01T00:00:00Z) to it we need to add 11644473600 seconds.
      //
      uint64_t ticks (chrono::duration_cast<chrono::nanoseconds> (
                        t.time_since_epoch ()).count ());

      ticks /= 100;                       // Now in 100 nanosecond "ticks".
      ticks += 11644473600ULL * 10000000; // Now in "Windows epoch".

      ft.dwHighDateTime = (ticks >> 32) & 0xFFFFFFFF;
      ft.dwLowDateTime = ticks & 0xFFFFFFFF;
      return &ft;
    };

    FILETIME at;
    FILETIME mt;
    if (!SetFileTime (h, NULL, tm (t.access, at), tm (t.modification, mt)))
      throw_system_error (GetLastError ());

#endif
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

  permissions
  path_permissions (const path& p)
  {
#ifndef _WIN32
    struct stat s;
    if (stat (p.string ().c_str (), &s) != 0)
#else
    struct _stat s;
    if (_stat (p.string ().c_str (), &s) != 0)
#endif
      throw_generic_error (errno);

    // VC++ has no S_IRWXU defined. MINGW GCC <= 4.9 has no S_IRWXG, S_IRWXO
    // defined.
    //
    // We could extrapolate user permissions to group/other permissions if
    // S_IRWXG/S_IRWXO are undefined. That is, we could consider their absence
    // as meaning that the platform does not distinguish between permissions
    // for different kinds of users. Let's wait for a use-case first.
    //
    mode_t f (S_IREAD | S_IWRITE | S_IEXEC);

#ifdef S_IRWXG
    f |= S_IRWXG;
#endif

#ifdef S_IRWXO
    f |= S_IRWXO;
#endif

    return static_cast<permissions> (s.st_mode & f);
  }

  void
  path_permissions (const path& p, permissions f)
  {
    mode_t m (S_IREAD | S_IWRITE | S_IEXEC);

#ifdef S_IRWXG
    m |= S_IRWXG;
#endif

#ifdef S_IRWXO
    m |= S_IRWXO;
#endif

    m &= static_cast<mode_t> (f);

#ifndef _WIN32
    if (chmod (p.string ().c_str (), m) == -1)
#else
    if (_chmod (p.string ().c_str (), m) == -1)
#endif
      throw_generic_error (errno);
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

      ignore_dangling_ = x.ignore_dangling_;
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
  type (bool link) const
  {
    path_type p (b_ / p_);
    struct stat s;
    if ((link
         ? stat (p.string ().c_str (), &s)
         : lstat (p.string ().c_str (), &s)) != 0)
      throw_generic_error (errno);

    return butl::type (s);
  }

  // dir_iterator
  //
  struct dir_deleter
  {
    void operator() (DIR* p) const {if (p != nullptr) closedir (p);}
  };

  dir_iterator::
  dir_iterator (const dir_path& d, bool ignore_dangling)
    : ignore_dangling_ (ignore_dangling)
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
  static inline /*constexpr*/ entry_type
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

    default: return entry_type::unknown;
    }
  }

  template <typename D>
  static inline constexpr entry_type
  d_type (...) {return entry_type::unknown;}

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
        e_.lt_ = entry_type::unknown;

        // If requested, we ignore dangling symlinks, skipping ones with
        // non-existing or inaccessible targets.
        //
        // Note that ltype () can potentially lstat() (see d_type() for
        // details) and so throw.
        //
        if (ignore_dangling_ && e_.ltype () == entry_type::symlink)
        {
          struct stat s;
          path pe (e_.base () / e_.path ());

          if (stat (pe.string ().c_str (), &s) != 0)
          {
            if (errno == ENOENT || errno == ENOTDIR || errno == EACCES)
              continue;

            throw_generic_error (errno);
          }

          e_.lt_ = type (s); // While at it, set the target type.
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
  dir_iterator::
  ~dir_iterator ()
  {
    if (h_ != -1)
      _findclose (h_); // Ignore any errors.
  }

  dir_iterator& dir_iterator::
  operator= (dir_iterator&& x)
  {
    if (this != &x)
    {
      e_ = move (x.e_);

      if (h_ != -1 && _findclose (h_) == -1)
        throw_generic_error (errno);

      h_ = x.h_;
      x.h_ = -1;

      ignore_dangling_ = x.ignore_dangling_;
    }
    return *this;
  }

  entry_type dir_entry::
  type (bool link) const
  {
    path_type p (base () / path ());
    pair<bool, entry_stat> e (path_entry (p, link));

    if (!e.first)
      throw_generic_error (ENOENT);

    return e.second.type;
  }

  // dir_iterator
  //
  struct auto_dir
  {
    explicit
    auto_dir (intptr_t& h): h_ (&h) {}

    auto_dir (const auto_dir&) = delete;
    auto_dir& operator= (const auto_dir&) = delete;

    ~auto_dir ()
    {
      if (h_ != nullptr && *h_ != -1)
        _findclose (*h_);
    }

    void release () {h_ = nullptr;}

  private:
    intptr_t* h_;
  };

  dir_iterator::
  dir_iterator (const dir_path& d, bool ignore_dangling)
    : ignore_dangling_ (ignore_dangling)
  {
    auto_dir h (h_);
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
      _finddata_t fi;

      if (h_ == -1)
      {
        // The call is made from the constructor. Any other call with h_ == -1
        // is illegal.
        //

        // Check to distinguish non-existent vs empty directories.
        //
        if (!dir_exists (e_.base ()))
          throw_generic_error (ENOENT);

        h_ = _findfirst ((e_.base () / path ("*")).string ().c_str (), &fi);
        r = h_ != -1;
      }
      else
        r = _findnext (h_, &fi) == 0;

      if (r)
      {
        // We can accept some overhead for '.' and '..' (relying on short
        // string optimization) in favor of a more compact code.
        //
        path p (fi.name);

        // Skip '.' and '..'.
        //
        if (p.current () || p.parent ())
          continue;

        e_.p_ = move (p);

        // An entry with the _A_SUBDIR attribute can also be a junction.
        //
        e_.t_ = (fi.attrib & _A_SUBDIR) == 0       ? entry_type::regular :
                junction (e_.base () / e_.path ()) ? entry_type::symlink :
                entry_type::directory;

        e_.lt_ = entry_type::unknown;

        // If requested, we ignore dangling symlinks, skipping ones with
        // non-existing or inaccessible targets.
        //
        if (ignore_dangling_                   &&
            e_.ltype () == entry_type::symlink &&
            !junction_target_exists (e_.base () / e_.path (),
                                     true /* ignore_error */))
          continue;
      }
      else if (errno == ENOENT)
      {
        // End of stream.
        //
        if (h_ != -1)
        {
          _findclose (h_);
          h_ = -1;
        }
      }
      else
        throw_generic_error (errno);

      break;
    }
  }
#endif

  // Search for paths matching the pattern and call the specified function for
  // each matching path. Return false if the underlying func() call returns
  // false. Otherwise the function conforms to the path_search() description.
  //
  // Note that the access to the traversed directory tree (real or virtual) is
  // performed through the provided filesystem object.
  //
  static const string any_dir ("*/");

  template <typename FS>
  static bool
  search (
    path pattern,
    dir_path pattern_dir,
    path_match_flags fl,
    const function<bool (path&&, const string& pattern, bool interm)>& func,
    FS& filesystem)
  {
    bool follow_symlinks ((fl & path_match_flags::follow_symlinks) !=
                          path_match_flags::none);

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

    // Note that we rely on "small function object" optimization here.
    //
    typename FS::iterator_type i (filesystem.iterator (
      pattern_dir,
      path_pattern_recursive (pcr),
      path_pattern_self_matching (pcr),
      follow_symlinks || !simple,
      [&pattern_dir, &func] (const dir_path& p) -> bool // Preopen.
      {
        return func (pattern_dir / p, any_dir, true);
      }));

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
      // start_dir.  We don't expect the start_dir to be empty, as the
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
                              filesystem))
        return false;
    }

    // If requested, also search with the absent-matching pattern path
    // component omitted, unless this is the only pattern component.
    //
    if ((fl & path_match_flags::match_absent) != path_match_flags::none &&
        pc.to_directory ()                                              &&
        (!pattern_dir.empty () || !simple)                              &&
        pc.string ().find_first_not_of ('*') == string::npos            &&
        !search (pattern.leaf (pc), pattern_dir, fl, func, filesystem))
      return false;

    return true;
  }

  // Path search implementations.
  //
  static const dir_path empty_dir;

  using preopen = function<bool (const dir_path&)>;

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
                            preopen po)
        : start_ (move (p)),
          recursive_ (recursive),
          self_ (self),
          follow_symlinks_ (fs),
          preopen_ (move (po))
    {
      open (dir_path (), self_);
    }

    // Move constructible-only, non-assignable type.
    //
    recursive_dir_iterator (const recursive_dir_iterator&) = delete;
    recursive_dir_iterator& operator= (const recursive_dir_iterator&) = delete;
    recursive_dir_iterator (recursive_dir_iterator&&) = default;

    // Return false if no more entries left. Otherwise save the next entry path
    // and return true. The path is relative against the directory being
    // traversed and contains a trailing separator for sub-directories. Throw
    // std::system_error in case of a failure (insufficient permissions,
    // dangling symlink encountered, etc).
    //
    bool
    next (path& p)
    {
      if (iters_.empty ())
        return false;

      auto& i (iters_.back ());

      // If we got to the end of directory sub-entries, then go one level up
      // and return this directory path.
      //
      if (i.first == dir_iterator ())
      {
        path d (move (i.second));
        iters_.pop_back ();

        // Return the path unless it is the last one (the directory we started
        // to iterate from) and the self flag is not set.
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

          // If we follow symlinks, then we ignore the dangling ones.
          //
          i = dir_iterator (!d.empty () ? d : dir_path ("."),
                            follow_symlinks_);
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
        assert (e.code ().category () == generic_category ());

        int ec (e.code ().value ());
        if (ec != ENOENT && ec != ENOTDIR)
          throw;
      }
    }

  private:
    dir_path start_;
    bool recursive_;
    bool self_;
    bool follow_symlinks_;
    preopen preopen_;
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
              preopen po) const
    {
      return iterator_type (start_ / p,
                            recursive,
                            self,
                            follow_symlinks,
                            move (po));
    }
  };

  void
  path_search (
    const path& pattern,
    const function<bool (path&&, const string& pattern, bool interm)>& func,
    const dir_path& start,
    path_match_flags flags)
  {
    real_filesystem fs (pattern.relative () ? start : empty_dir);
    search (pattern, dir_path (), flags, func, fs);
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
              preopen po)
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
    search (pattern, dir_path (), flags, func, fs);
  }
}
