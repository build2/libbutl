// file      : butl/filesystem.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/filesystem>

#ifndef _WIN32
#  include <stdio.h>     // rename()
#  include <dirent.h>    // struct dirent, *dir()
#  include <unistd.h>    // symlink(), link(), stat(), rmdir(), unlink()
#  include <sys/time.h>  // utimes()
#  include <sys/types.h> // stat
#  include <sys/stat.h>  // stat(), lstat(), S_I*, mkdir(), chmod()
#else
#  include <butl/win32-utility>

#  include <io.h>        // _find*(), _unlink(), _chmod()
#  include <direct.h>    // _mkdir(), _rmdir()
#  include <sys/types.h> // _stat
#  include <sys/stat.h>  // _stat(), S_I*

#  ifdef _MSC_VER // Unlikely to be fixed in newer versions.
#    define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#    define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#  endif

#  include <butl/utility> // lcase()
#endif

#include <errno.h> // errno, E*

#include <string>
#include <vector>
#include <memory>       // unique_ptr
#include <utility>      // pair
#include <iterator>     // reverse_iterator
#include <system_error>

#include <butl/path>
#include <butl/utility>      // throw_generic_error()
#include <butl/fdstream>
#include <butl/small-vector>

using namespace std;

namespace butl
{
  bool
  file_exists (const char* p, bool fl)
  {
    auto pe (path_entry (p, fl));
    return pe.first && (pe.second == entry_type::regular ||
                        (!fl && pe.second == entry_type::symlink));
  }

  bool
  entry_exists (const char* p, bool fl)
  {
    return path_entry (p, fl).first;
  }

  bool
  dir_exists (const char* p)
  {
    auto pe (path_entry (p, true));
    return pe.first && pe.second == entry_type::directory;
  }

#ifndef _WIN32
  pair<bool, entry_type>
  path_entry (const char* p, bool fl)
  {
    struct stat s;
    if ((fl ? stat (p, &s) : lstat (p, &s)) != 0)
    {
      if (errno == ENOENT || errno == ENOTDIR)
        return make_pair (false, entry_type::unknown);
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

    return make_pair (true, t);
  }
#else
  pair<bool, entry_type>
  path_entry (const char* p, bool)
  {
    // A path like 'C:', while being a root path in our terminology, is not as
    // such for Windows, that maintains current directory for each drive, and
    // so C: means the current directory on the drive C. This is not what we
    // mean here, so need to append the trailing directory separator in such a
    // case.
    //
    string d;
    if (path::traits::root (p, string::traits_type::length (p)))
    {
      d = p;
      d += path::traits::directory_separator;
      p = d.c_str ();
    }

    DWORD attr (GetFileAttributesA (p));
    if (attr == INVALID_FILE_ATTRIBUTES) // Presumably not exists.
      return make_pair (false, entry_type::unknown);

    entry_type t (entry_type::unknown);

    // S_ISLNK/S_IFDIR are not defined for Win32 but it does have symlinks.
    // We will consider symlink entry to be of the unknown type. Note that
    // S_ISREG() and S_ISDIR() return as they would do for a symlink target.
    //
    if ((attr & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
    {
      struct _stat s;

      if (_stat (p, &s) != 0)
      {
        if (errno == ENOENT || errno == ENOTDIR)
          return make_pair (false, entry_type::unknown);
        else
          throw_generic_error (errno);
      }

      auto m (s.st_mode);

      if (S_ISREG (m))
        t = entry_type::regular;
      else if (S_ISDIR (m))
        t = entry_type::directory;
      //
      //else if (S_ISLNK (m))
      //  t = entry_type::symlink;
    }

    return make_pair (true, t);
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

      // EEXIST means the path already exists but not necessarily as
      // a directory.
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
    for (const dir_entry& de: dir_iterator (p))
    {
      path ep (p / de.path ()); //@@ Would be good to reuse the buffer.

      if (de.ltype () == entry_type::directory)
        rmdir_r (path_cast<dir_path> (move (ep)), true, ignore_error);
      else
        try_rmfile (ep, ignore_error);
    }

    if (dir)
    {
      rmdir_status r (try_rmdir (p));

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

#ifndef _WIN32
    if (unlink (p.string ().c_str ()) != 0)
#else
    if (_unlink (p.string ().c_str ()) != 0)
#endif
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

  void
  mkhardlink (const path& target, const path& link, bool)
  {
    if (::link (target.string ().c_str (), link.string ().c_str ()) == -1)
      throw_generic_error (errno);
  }

#else

  void
  mksymlink (const path&, const path&, bool)
  {
    throw_generic_error (ENOSYS, "symlinks not supported");
  }

  void
  mkhardlink (const path& target, const path& link, bool dir)
  {
    if (!dir)
    {
      if (!CreateHardLinkA (link.string ().c_str (),
                            target.string ().c_str (),
                            nullptr))
        throw_system_error (GetLastError ());
    }
    else
      throw_generic_error (ENOSYS, "directory hard links not supported");
  }
#endif

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

    rm.cancel ();
  }

  // Figuring out whether we have the nanoseconds in struct stat. Some
  // platforms (e.g., FreeBSD), may provide some "compatibility" #define's,
  // so use the second argument to not end up with the same signatures.
  //
  template <typename S>
  inline constexpr auto
  mnsec (const S* s, bool) -> decltype(s->st_mtim.tv_nsec)
  {
    return s->st_mtim.tv_nsec; // POSIX (GNU/Linux, Solaris).
  }

  template <typename S>
  inline constexpr auto
  mnsec (const S* s, int) -> decltype(s->st_mtimespec.tv_nsec)
  {
    return s->st_mtimespec.tv_nsec; // *BSD, MacOS.
  }

  template <typename S>
  inline constexpr auto
  mnsec (const S* s, float) -> decltype(s->st_mtime_n)
  {
    return s->st_mtime_n; // AIX 5.2 and later.
  }

  // Things are not going to end up well with only seconds resolution so
  // let's make it a compile error.
  //
  // template <typename S>
  // inline constexpr int
  // mnsec (...) {return 0;}

  template <typename S>
  inline constexpr auto
  ansec (const S* s, bool) -> decltype(s->st_atim.tv_nsec)
  {
    return s->st_atim.tv_nsec; // POSIX (GNU/Linux, Solaris).
  }

  template <typename S>
  inline constexpr auto
  ansec (const S* s, int) -> decltype(s->st_atimespec.tv_nsec)
  {
    return s->st_atimespec.tv_nsec; // *BSD, MacOS.
  }

  template <typename S>
  inline constexpr auto
  ansec (const S* s, float) -> decltype(s->st_atime_n)
  {
    return s->st_atime_n; // AIX 5.2 and later.
  }

  // template <typename S>
  // inline constexpr int
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
    struct stat s;
    if (stat (f, &s) != 0)
      throw_generic_error (errno);

    timeval times[2];
    times[0].tv_sec = s.st_atime;
    times[0].tv_usec = ansec<struct stat> (&s, true) / 1000;
    times[1].tv_sec = s.st_mtime;
    times[1].tv_usec = mnsec<struct stat> (&s, true) / 1000;

    if (utimes (t, times) != 0)
      throw_generic_error (errno);

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

    bool td (te.first && te.second == entry_type::directory);

    auto fe (path_entry (from));
    bool fd (fe.first && fe.second == entry_type::directory);

    // If source and destination filesystem entries exist, they both must be
    // either directories or not directories.
    //
    if (fe.first && te.first && fd != td)
      throw_generic_error (ENOTDIR);

    DWORD mfl (fd ? 0 : (MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING));

    if (MoveFileExA (f, t, mfl))
      return;

    // If the destination already exists, then MoveFileExA() succeeds only if
    // it is a regular file or a symlink. Lets also support an empty directory
    // special case to comply with POSIX. If the destination is an empty
    // directory we will just remove it and retry the move operation.
    //
    // Note that under Wine we endup with ERROR_ACCESS_DENIED error code in
    // that case, and with ERROR_ALREADY_EXISTS when run natively.
    //
    DWORD ec (GetLastError ());
    if ((ec == ERROR_ALREADY_EXISTS || ec == ERROR_ACCESS_DENIED) && td &&
	try_rmdir (path_cast<dir_path> (to)) != rmdir_status::not_empty &&
	MoveFileExA (f, t, mfl))
      return;

    throw_system_error (ec);

#endif
  }

  timestamp
  file_mtime (const char* p)
  {
#ifndef _WIN32
    struct stat s;
    if (stat (p, &s) != 0)
    {
      if (errno == ENOENT || errno == ENOTDIR)
        return timestamp_nonexistent;
      else
        throw_generic_error (errno);
    }

    if (!S_ISREG (s.st_mode))
      return timestamp_nonexistent;

    return system_clock::from_time_t (s.st_mtime) +
      chrono::duration_cast<duration> (
        chrono::nanoseconds (mnsec<struct stat> (&s, true)));
#else

    WIN32_FILE_ATTRIBUTE_DATA s;

    if (!GetFileAttributesExA (p, GetFileExInfoStandard, &s))
    {
      DWORD ec (GetLastError ());

      if (ec == ERROR_FILE_NOT_FOUND ||
          ec == ERROR_PATH_NOT_FOUND ||
          ec == ERROR_INVALID_NAME   ||
          ec == ERROR_INVALID_DRIVE  ||
          ec == ERROR_BAD_PATHNAME   ||
          ec == ERROR_BAD_NETPATH)
        return timestamp_nonexistent;

      throw_system_error (ec);
    }

    if ((s.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
      return timestamp_nonexistent;

    // Time in FILETIME is in 100 nanosecond "ticks" since "Windows epoch"
    // (1601-01-01T00:00:00Z). To convert it to "UNIX epoch"
    // (1970-01-01T00:00:00Z) we need to subtract 11644473600 seconds.
    //
    const FILETIME& t (s.ftLastWriteTime);

    uint64_t ns ((static_cast<uint64_t> (t.dwHighDateTime) << 32) |
                 t.dwLowDateTime);

    ns -= 11644473600ULL * 10000000; // Now in UNIX epoch.
    ns *= 100;                       // Now in nanoseconds.

    return timestamp (
      chrono::duration_cast<duration> (
        chrono::nanoseconds (ns)));
#endif
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
    }
    return *this;
  }

  entry_type dir_entry::
  type (bool link) const
  {
    path_type p (b_ / p_);
    struct stat s;
    if ((link
         ? stat (p.string ().c_str (), &s)
         : lstat (p.string ().c_str (), &s)) != 0)
    {
      throw_generic_error (errno);
    }

    entry_type r;

    if (S_ISREG (s.st_mode))
      r = entry_type::regular;
    else if (S_ISDIR (s.st_mode))
      r = entry_type::directory;
    else if (S_ISLNK (s.st_mode))
      r = entry_type::symlink;
    else
      r = entry_type::other;

    return r;
  }

  // dir_iterator
  //
  struct dir_deleter
  {
    void operator() (DIR* p) const {if (p != nullptr) closedir (p);}
  };

  dir_iterator::
  dir_iterator (const dir_path& d)
  {
    unique_ptr<DIR, dir_deleter> h (opendir (d.string ().c_str ()));
    h_ = h.get ();

    if (h_ == nullptr)
      throw_generic_error (errno);

    next ();

    if (h_ != nullptr)
      e_.b_ = d;

    h.release ();
  }

  template <typename D>
  inline /*constexpr*/ entry_type d_type (const D* d, decltype(d->d_type)*)
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
  inline constexpr entry_type d_type (...) {return entry_type::unknown;}

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
    }
    return *this;
  }

  entry_type dir_entry::
  type (bool) const
  {
    // Note that we currently do not support symlinks (yes, there is symlink
    // support since Vista).
    //
    path_type p (b_ / p_);

    struct _stat s;
    if (_stat (p.string ().c_str (), &s) != 0)
      throw_generic_error (errno);

    entry_type r;
    if (S_ISREG (s.st_mode))
      r = entry_type::regular;
    else if (S_ISDIR (s.st_mode))
      r = entry_type::directory;
    else
      r = entry_type::other;

    return r;
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
  dir_iterator (const dir_path& d)
  {
    auto_dir h (h_);
    e_.b_ = d; // Used by next() to call _findfirst().

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
        if (!dir_exists (e_.b_))
          throw_generic_error (ENOENT);

        h_ = _findfirst ((e_.b_ / path ("*")).string ().c_str (), &fi);
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

        // We do not support symlinks at the moment.
        //
        e_.t_ = fi.attrib & _A_SUBDIR
          ? entry_type::directory
          : entry_type::regular;

        e_.lt_ = entry_type::unknown;
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

  // Match the name [ni, ne) to the pattern [pi, pe). Ranges can be empty.
  //
  static bool
  match (string::const_iterator pi, string::const_iterator pe,
         string::const_iterator ni, string::const_iterator ne)
  {
    using reverse_iterator = std::reverse_iterator<string::const_iterator>;

    reverse_iterator rpi (pe);
    reverse_iterator rpe (pi);

    reverse_iterator rni (ne);
    reverse_iterator rne (ni);

    // Match the pattern suffix (follows the last *) to the name trailing
    // characters.
    //
    char pc;
    for (; rpi != rpe && (pc = *rpi) != '*' && rni != rne; ++rpi, ++rni)
    {
#ifndef _WIN32
      if (*rni != pc && pc != '?')
#else
      if (lcase (*rni) != lcase (pc) && pc != '?')
#endif
        return false;
    }

    // If we got to the (reversed) end of the pattern (no * is encountered)
    // than we are done. The success depends on if we got to the (reversed) end
    // of the name as well.
    //
    if (rpi == rpe)
      return rni == rne;

    // If we didn't reach * in the pattern then we reached the (reversed) end
    // of the name. That means we have unmatched non-star characters in the
    // pattern, and so match failed.
    //
    if (pc != '*')
    {
      assert (rni == rne);
      return false;
    }

    // Match the pattern prefix (ends with the first *) to the name leading
    // characters. If they mismatch we failed. Otherwise if this is an only *
    // in the pattern (matches whatever is left in the name) then we succeed,
    // otherwise we perform backtracking (recursively).
    //
    pe = rpi.base ();
    ne = rni.base ();

    // Compare the pattern and the name char by char until the name suffix or
    // * is encountered in the pattern (whichever happens first). Fail if a
    // char mismatches.
    //
    for (; (pc = *pi) != '*' && ni != ne; ++pi, ++ni)
    {
#ifndef _WIN32
      if (*ni != pc && pc != '?')
#else
        if (lcase (*ni) != lcase (pc) && pc != '?')
#endif
        return false;
    }

    // If we didn't get to * in the pattern then we got to the name suffix.
    // That means that the pattern has unmatched non-star characters, and so
    // match failed.
    //
    if (pc != '*')
    {
      assert (ni == ne);
      return false;
    }

    // If * that we have reached is the last one, then it matches whatever is
    // left in the name (including an empty range).
    //
    if (++pi == pe)
      return true;

    // Perform backtracking.
    //
    // From now on, we will call the pattern not-yet-matched part (starting
    // the leftmost * and ending the rightmost one inclusively) as pattern, and
    // the name not-yet-matched part as name.
    //
    // Here we sequentially assume that * that starts the pattern matches the
    // name leading part (staring from an empty one and iterating till the full
    // name). So if, at some iteration, the pattern trailing part (that follows
    // the leftmost *) matches the name trailing part, then the pattern matches
    // the name.
    //
    bool r;
    for (; !(r = match (pi, pe, ni, ne)) && ni != ne; ++ni) ;
    return r;
  }

  bool
  path_match (const string& pattern, const string& name)
  {
    // Implementation notes:
    //
    // - This has a good potential of becoming hairy quickly so need to strive
    //   for an elegant way to implement this.
    //
    // - Most patterns will contains a single * wildcard with a prefix and/or
    //   suffix (e.g., *.txt, foo*, f*.txt). Something like this is not very
    //   common: *foo*.
    //
    //   So it would be nice to have a clever implementation that first
    //   "anchors" itself with a literal prefix and/or suffix and only then
    //   continue with backtracking. In other words, reduce:
    //
    //   *.txt  vs foo.txt -> * vs foo
    //   foo*   vs foo.txt -> * vs .txt
    //   f*.txt vs foo.txt -> * vs oo
    //

    auto pi (pattern.rbegin ());
    auto pe (pattern.rend ());

    auto ni (name.rbegin ());
    auto ne (name.rend ());

    // The name doesn't match the pattern if it is of a different type than the
    // pattern is.
    //
    bool pd (pi != pe && path::traits::is_separator (*pi));
    bool nd (ni != ne && path::traits::is_separator (*ni));

    if (pd != nd)
      return false;

    // Skip trailing separators if present.
    //
    if (pd)
    {
      ++pi;
      ++ni;
    }

    return match (pattern.begin (), pi.base (), name.begin (), ni.base ());
  }

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
  using preopen = std::function<bool (const dir_path&)>;

  class recursive_dir_iterator
  {
  public:
    recursive_dir_iterator (dir_path p,
                            bool recursive,
                            bool self,
                            preopen po)
        : start_ (move (p)),
          recursive_ (recursive),
          self_ (self),
          preopen_ (move (po))
    {
      open (dir_path (), self_);
    }

    // Non-copyable, non-movable type.
    //
    recursive_dir_iterator (const recursive_dir_iterator&) = delete;
    recursive_dir_iterator& operator= (const recursive_dir_iterator&) = delete;

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
      path pe (de.type () == entry_type::directory
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
        // traversed (as we leave iterator with end semantics) but still be
        // returned by the next() call as a sub-entry.
        //
        dir_iterator i;
        if (!preopen || preopen_ (p))
        {
          dir_path d (start_ / p);
          i = dir_iterator (!d.empty () ? d : dir_path ("."));
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
    preopen preopen_;
    small_vector<pair<dir_iterator, dir_path>, 1> iters_;
  };

  // Search for paths matching the pattern and call the specified function for
  // each matching path. Return false if the underlying func() call returns
  // false. Otherwise the function conforms to the path_search() description.
  //
  static const string any_dir ("*/");

  static bool
  search (
    path pattern,
    dir_path pattern_dir,
    const dir_path start_dir,
    const function<bool (path&&, const string& pattern, bool interm)>& func)
  {
    // Fast-forward the leftmost pattern non-wildcard components. So, for
    // example, search for foo/f* in /bar/ becomes search for f* in /bar/foo/.
    //
    {
      auto b (pattern.begin ());
      auto e (pattern.end ());
      auto i (b);
      for (; i != e && (*i).find_first_of ("*?") == string::npos; ++i) ;

      // If the pattern has no wildcards then we reduce to checking for the
      // filesystem entry existence. It matches if exists and is of the proper
      // type.
      //
      if (i == e)
      {
        path p (pattern_dir / pattern);
        auto pe (path_entry (start_dir / p, true));

        if (pe.first &&
            ((pe.second == entry_type::directory) == p.to_directory ()))
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
    recursive_dir_iterator i (
      start_dir / pattern_dir,
      pcr.find ("**") != string::npos,                  // Recursive.
      pcr.find ("***") != string::npos,                 // Self-inclusive.
      [&pattern_dir, &func] (const dir_path& p) -> bool // Preopen.
      {
        return func (pattern_dir / p, any_dir, true);
      });

    // Canonicalize the pattern component collapsing consecutive stars (used to
    // express that it is recursive) into a single one.
    //
    size_t j (0);
    size_t n (pcr.size ());
    for (size_t i (0); i < n; ++i)
    {
      char c (pcr[i]);
      if (!(c == '*' && i > 0 && pcr[i - 1] == '*'))
        pcr[j++] = c;
    }

    if (j != n)
      pcr.resize (j);

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
      // leaf can be aquired from the pattern_dir / start_dir path except the
      // case when both directories are empty. This is the case when we search
      // in the current directory (start_dir is empty) with a pattern that
      // starts with *** wildcard (for example f***/bar). All we can do here is
      // to fallback to path::current_directory() call. Note that this will be
      // the only call per path_search() as the next time pattern_dir will not
      // be empty.
      //
      const path& se (!p.empty ()
                      ? p
                      : path_cast<path> (!pattern_dir.empty ()
                                         ? pattern_dir
                                         : !start_dir.empty ()
                                           ? start_dir
                                           : path::current_directory ()));

      if (!path_match (pcr, se.leaf ().representation ()))
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
                              start_dir,
                              func))
        return false;
    }

    return true;
  }

  void
  path_search (
    const path& pattern,
    const function<bool (path&&, const string& pattern, bool interm)>& func,
    const dir_path& start)
  {
    search (pattern,
            dir_path (),
            pattern.relative () ? start : dir_path (),
            func);
  }
}
