// file      : butl/filesystem.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2015 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/filesystem>

#include <unistd.h>    // stat, rmdir(), unlink()
#include <sys/types.h> // stat
#include <sys/stat.h>  // stat, lstat(), S_IS*, mkdir()

#include <system_error>

using namespace std;

namespace butl
{
  // Figuring out whether we have the nanoseconds in some form.
  //
  template <typename S>
  constexpr auto nsec (const S* s) -> decltype(s->st_mtim.tv_nsec)
  {
    return s->st_mtim.tv_nsec; // POSIX (GNU/Linux, Solaris).
  }

  template <typename S>
  constexpr auto nsec (const S* s) -> decltype(s->st_mtimespec.tv_nsec)
  {
    return s->st_mtimespec.tv_nsec; // MacOS X.
  }

  template <typename S>
  constexpr auto nsec (const S* s) -> decltype(s->st_mtime_n)
  {
    return s->st_mtime_n; // AIX 5.2 and later.
  }

  template <typename S>
  constexpr int nsec (...) {return 0;}

  timestamp
  file_mtime (const path& p)
  {
    struct stat s;
    if (::lstat (p.string ().c_str (), &s) != 0)
    {
      if (errno == ENOENT || errno == ENOTDIR)
        return timestamp_nonexistent;
      else
        throw system_error (errno, system_category ());
    }

    return S_ISREG (s.st_mode)
      ? system_clock::from_time_t (s.st_mtime) +
      chrono::duration_cast<duration> (
        chrono::nanoseconds (nsec<struct stat> (&s)))
      : timestamp_nonexistent;
  }

  bool
  dir_exists (const path& p)
  {
    struct stat s;
    if (::lstat (p.string ().c_str (), &s) != 0)
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
    if (::lstat (p.string ().c_str (), &s) != 0)
    {
      if (errno == ENOENT || errno == ENOTDIR)
        return false;
      else
        throw system_error (errno, system_category ());
    }

    return S_ISREG (s.st_mode);
  }

  mkdir_status
  try_mkdir (const path& p, mode_t m)
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

  rmdir_status
  try_rmdir (const path& p)
  {
    rmdir_status r (rmdir_status::success);

    if (::rmdir (p.string ().c_str ()) != 0)
    {
      if (errno == ENOENT)
        r = rmdir_status::not_exist;
      else if (errno == ENOTEMPTY || errno == EEXIST)
        r = rmdir_status::not_empty;
      else
        throw system_error (errno, system_category ());
    }

    return r;
  }

  rmfile_status
  try_rmfile (const path& p)
  {
    rmfile_status r (rmfile_status::success);

    if (::unlink (p.string ().c_str ()) != 0)
    {
      if (errno == ENOENT || errno == ENOTDIR)
        r = rmfile_status::not_exist;
      else
        throw system_error (errno, system_category ());
    }

    return r;
  }
}
