// file      : butl/filesystem.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/filesystem>

#include <unistd.h>    // stat, rmdir(), unlink()
#include <sys/types.h> // stat
#include <sys/stat.h>  // stat, lstat(), S_IS*, mkdir()

#include <memory>       // unique_ptr
#include <system_error>

using namespace std;

namespace butl
{
  bool
  dir_exists (const path& p)
  {
    struct stat s;
    if (::stat (p.string ().c_str (), &s) != 0)
    {
      if (errno == ENOENT || errno == ENOTDIR)
        return false;
      else
        throw system_error (errno, system_category ());
    }

    return S_ISDIR (s.st_mode);
  }

  bool
  file_exists (const path& p)
  {
    struct stat s;
    if (::stat (p.string ().c_str (), &s) != 0)
    {
      if (errno == ENOENT || errno == ENOTDIR)
        return false;
      else
        throw system_error (errno, system_category ());
    }

    return S_ISREG (s.st_mode);
  }

  mkdir_status
  try_mkdir (const dir_path& p, mode_t m)
  {
    mkdir_status r (mkdir_status::success);

    if (::mkdir (p.string ().c_str (), m) != 0)
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

    return r;
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

    if (::rmdir (p.string ().c_str ()) != 0)
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
  rmdir_r (const dir_path& p, bool dir)
  {
    // An nftw()-based implementation (for platforms that support it)
    // might be a faster way.
    //
    for (const dir_entry& de: dir_iterator (p))
    {
      path ep (p / de.path ()); //@@ Would be good to reuse the buffer.

      if (de.ltype () == entry_type::directory)
        rmdir_r (path_cast<dir_path> (ep));
      else
        try_rmfile (ep);
    }

    if (dir)
    {
      rmdir_status r (try_rmdir (p));

      if (r != rmdir_status::success)
        throw system_error (r == rmdir_status::not_empty ? ENOTEMPTY : ENOENT,
                            system_category ());
    }
  }

  rmfile_status
  try_rmfile (const path& p, bool ignore_error)
  {
    rmfile_status r (rmfile_status::success);

    if (::unlink (p.string ().c_str ()) != 0)
    {
      if (errno == ENOENT || errno == ENOTDIR)
        r = rmfile_status::not_exist;
      else if (!ignore_error)
        throw system_error (errno, system_category ());
    }

    return r;
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
    struct stat s;
    if (::stat (p.string ().c_str (), &s) != 0)
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
    struct stat s;
    if (::stat (p.string ().c_str (), &s) != 0)
      throw system_error (errno, system_category ());

    return static_cast<permissions> (
      s.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));
  }

#ifndef _WIN32

  // dir_entry
  //
  entry_type dir_entry::
  type (bool link) const
  {
    path_type p (b_ / p_);
    struct stat s;
    if ((link
         ? ::stat (p.string ().c_str (), &s)
         : ::lstat (p.string ().c_str (), &s)) != 0)
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
    void operator() (DIR* p) const {if (p != nullptr) ::closedir (p);}
  };

  dir_iterator::
  dir_iterator (const dir_path& d)
  {
    unique_ptr<DIR, dir_deleter> h (::opendir (d.string ().c_str ()));
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
      if (struct dirent* de = ::readdir (h_))
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
        ::closedir (h_);
        h_ = nullptr;
      }
      else
        throw system_error (errno, system_category ());

      break;
    }
  }
#else
#endif
}
