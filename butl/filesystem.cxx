// file      : butl/filesystem.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/filesystem>

#ifndef _WIN32
#  include <dirent.h>    // struct dirent, *dir()
#  include <unistd.h>    // symlink(), link(), stat(), rmdir(), unlink()
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
#endif

#include <errno.h> // errno, E*

#include <memory>       // unique_ptr
#include <system_error>

#include <butl/fdstream>

using namespace std;

namespace butl
{
  bool
  file_exists (const path& p)
  {
#ifndef _WIN32
    struct stat s;
    if (stat (p.string ().c_str (), &s) != 0)
#else
    struct _stat s;
    if (_stat (p.string ().c_str (), &s) != 0)
#endif
    {
      if (errno == ENOENT || errno == ENOTDIR)
        return false;
      else
        throw system_error (errno, system_category ());
    }

    return S_ISREG (s.st_mode);
  }

  bool
  dir_exists (const path& p)
  {
#ifndef _WIN32
    struct stat s;
    if (stat (p.string ().c_str (), &s) != 0)
#else
    struct _stat s;
    if (_stat (p.string ().c_str (), &s) != 0)
#endif
    {
      if (errno == ENOENT || errno == ENOTDIR)
        return false;
      else
        throw system_error (errno, system_category ());
    }

    return S_ISDIR (s.st_mode);
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

      // EEXIST means the path already exists but not necessarily as
      // a directory.
      //
      if (e == EEXIST && dir_exists (p))
        return mkdir_status::already_exists;
      else
        throw system_error (e, system_category ());
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
        throw system_error (errno, system_category ());
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
        throw system_error (r == rmdir_status::not_empty ? ENOTEMPTY : ENOENT,
                            system_category ());
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
      if (errno == ENOENT || errno == ENOTDIR)
        r = rmfile_status::not_exist;
      else if (!ignore_error)
        throw system_error (errno, system_category ());
    }

    return r;
  }

#ifndef _WIN32
  void
  mksymlink (const path& target, const path& link, bool)
  {
    if (symlink (target.string ().c_str (), link.string ().c_str ()) == -1)
      throw system_error (errno, system_category ());
  }

  void
  mkhardlink (const path& target, const path& link, bool)
  {
    if (::link (target.string ().c_str (), link.string ().c_str ()) == -1)
      throw system_error (errno, system_category ());
  }

#else

  void
  mksymlink (const path&, const path&, bool)
  {
    throw system_error (ENOSYS, system_category (), "symlinks not supported");
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
        string e (win32::last_error_msg ());
        throw system_error (EIO, system_category (), e);
      }
    }
    else
      throw system_error (
        ENOSYS, system_category (), "directory hard links not supported");
  }
#endif

  void
  cpfile (const path& from, const path& to, cpflags fl)
  {
    permissions perm (path_permissions (from));
    ifdstream ifs (from, fdopen_mode::binary);

    fdopen_mode om (fdopen_mode::out      |
                    fdopen_mode::truncate |
                    fdopen_mode::create   |
                    fdopen_mode::binary);

    if ((fl & cpflags::overwrite_content) != cpflags::overwrite_content)
      om |= fdopen_mode::exclusive;

    // Create prior to the output file stream creation so that the file is
    // removed after it is closed.
    //
    auto_rmfile rm;

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
  nsec (const S* s, bool) -> decltype(s->st_mtim.tv_nsec)
  {
    return s->st_mtim.tv_nsec; // POSIX (GNU/Linux, Solaris).
  }

  template <typename S>
  inline constexpr auto
  nsec (const S* s, int) -> decltype(s->st_mtimespec.tv_nsec)
  {
    return s->st_mtimespec.tv_nsec; // *BSD, MacOS.
  }

  template <typename S>
  inline constexpr auto
  nsec (const S* s, float) -> decltype(s->st_mtime_n)
  {
    return s->st_mtime_n; // AIX 5.2 and later.
  }

  template <typename S>
  inline constexpr int
  nsec (...) {return 0;}

  timestamp
  file_mtime (const path& p)
  {
#ifndef _WIN32
    struct stat s;
    if (stat (p.string ().c_str (), &s) != 0)
#else
    struct _stat s;
    if (_stat (p.string ().c_str (), &s) != 0)
#endif
    {
      if (errno == ENOENT || errno == ENOTDIR)
        return timestamp_nonexistent;
      else
        throw system_error (errno, system_category ());
    }

    return S_ISREG (s.st_mode)
      ? system_clock::from_time_t (s.st_mtime) +
      chrono::duration_cast<duration> (
        chrono::nanoseconds (nsec<struct stat> (&s, true)))
      : timestamp_nonexistent;
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
      throw system_error (errno, system_category ());

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
      throw system_error (errno, system_category ());
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
        throw system_error (errno, system_category ());

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
      throw system_error (errno, system_category ());
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
      throw system_error (errno, system_category ());

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
        const char* n (de->d_name);

        // Skip '.' and '..'.
        //
        if (n[0] == '.' && (n[1] == '\0' || (n[1] == '.' && n[2] == '\0')))
          continue;

        e_.p_ = path (n);
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
        throw system_error (errno, system_category ());

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
        throw system_error (errno, system_category ());

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

    struct stat s;
    if (stat (p.string ().c_str (), &s) != 0)
      throw system_error (errno, system_category ());

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
          throw system_error (ENOENT, system_category ());

        h_ = _findfirst ((e_.b_ / path ("*")).string ().c_str (), &fi);
        r = h_ != -1;
      }
      else
        r = _findnext (h_, &fi) == 0;

      if (r)
      {
        const char* n (fi.name);

        // Skip '.' and '..'.
        //
        if (n[0] == '.' && (n[1] == '\0' || (n[1] == '.' && n[2] == '\0')))
          continue;

        e_.p_ = path (n);

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
        throw system_error (errno, system_category ());

      break;
    }
  }
#endif
}
