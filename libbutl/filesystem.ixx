// file      : libbutl/filesystem.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/utility.hxx> // operator<<(ostream,exception),
                               // throw_generic_error()

namespace butl
{
  inline bool
  dir_empty (const dir_path& d)
  {
    // @@ Could 0 size be a valid and faster way?
    //
    return dir_iterator (d, dir_iterator::no_follow) == dir_iterator ();
  }

  inline bool
  file_empty (const path& f)
  {
    auto p (path_entry (f, true /* follow_symlinks */));

    if (p.first && p.second.type == entry_type::regular)
      return p.second.size == 0;

    throw_generic_error (
      p.first
      ? (p.second.type == entry_type::directory ? EISDIR : EINVAL)
      : ENOENT);
  }

  inline rmdir_status
  try_rmdir_r (const dir_path& p, bool ignore_error)
  {
    //@@ What if it exists but is not a directory?
    //
    bool e (dir_exists (p, ignore_error));

    if (e)
      rmdir_r (p, true, ignore_error);

    return e ? rmdir_status::success : rmdir_status::not_exist;
  }

  LIBBUTL_SYMEXPORT optional<rmfile_status>
  try_rmfile_maybe_ignore_error (const path&, bool ignore_error);

  inline rmfile_status
  try_rmfile (const path& p, bool ignore_error)
  {
    auto r (try_rmfile_maybe_ignore_error (p, ignore_error));
    return r ? *r : rmfile_status::success;
  }

  inline optional<rmfile_status>
  try_rmfile_ignore_error (const path& p)
  {
    return try_rmfile_maybe_ignore_error (p, true);
  }


  inline path
  followsymlink (const path& p)
  {
    std::pair<path, bool> r (try_followsymlink (p));

    if (!r.second)
      throw_generic_error (ENOENT);

    return std::move (r.first);
  }

  // auto_rm
  //
  template <typename P>
  inline auto_rm<P>::
  auto_rm (auto_rm&& x) noexcept
      : path (std::move (x.path)), active (x.active)
  {
    x.active = false;
  }

  template <typename P>
  inline auto_rm<P>& auto_rm<P>::
  operator= (auto_rm&& x) noexcept
  {
    if (this != &x)
    {
      path = std::move (x.path);
      active = x.active;
      x.active = false;
    }

    return *this;
  }

  template <>
  inline auto_rm<path>::
  ~auto_rm () {if (active && !path.empty ()) try_rmfile (path, true);}

  template <>
  inline auto_rm<dir_path>::
  ~auto_rm () {if (active && !path.empty ()) try_rmdir_r (path, true);}

  // cpflags
  //
  inline cpflags operator& (cpflags x, cpflags y) {return x &= y;}
  inline cpflags operator| (cpflags x, cpflags y) {return x |= y;}
  inline cpflags operator&= (cpflags& x, cpflags y)
  {
    return x = static_cast<cpflags> (
      static_cast<std::uint16_t> (x) &
      static_cast<std::uint16_t> (y));
  }

  inline cpflags operator|= (cpflags& x, cpflags y)
  {
    return x = static_cast<cpflags> (
      static_cast<std::uint16_t> (x) |
      static_cast<std::uint16_t> (y));
  }

  // permissions
  //
  inline permissions operator& (permissions x, permissions y) {return x &= y;}
  inline permissions operator| (permissions x, permissions y) {return x |= y;}
  inline permissions operator&= (permissions& x, permissions y)
  {
    return x = static_cast<permissions> (
      static_cast<std::uint16_t> (x) &
      static_cast<std::uint16_t> (y));
  }

  inline permissions operator|= (permissions& x, permissions y)
  {
    return x = static_cast<permissions> (
      static_cast<std::uint16_t> (x) |
      static_cast<std::uint16_t> (y));
  }

  // dir_entry
  //
  inline entry_type dir_entry::
  ltype () const
  {
    return t_ ? *t_ : *(t_ = type (false /* follow_symlinks */));
  }

  inline entry_type dir_entry::
  type () const
  {
    entry_type t (ltype ());
    return t != entry_type::symlink ? t    :
           lt_                      ? *lt_ :
           *(lt_ = type (true /* follow_symlinks */));
  }

  // dir_iterator
  //
  inline dir_iterator::
  dir_iterator (dir_iterator&& x) noexcept
    : e_ (std::move (x.e_)), h_ (x.h_), mode_ (x.mode_)
  {
#ifndef _WIN32
    x.h_ = nullptr;
#else
    x.h_ = -1;
#endif
  }

  inline bool
  operator== (const dir_iterator& x, const dir_iterator& y)
  {
    return x.h_ == y.h_;
  }

  inline bool
  operator!= (const dir_iterator& x, const dir_iterator& y)
  {
    return !(x == y);
  }

  inline dir_iterator
  begin (dir_iterator& i)
  {
    return std::move (i);
  }

  inline dir_iterator
  end (const dir_iterator&)
  {
    return dir_iterator ();
  }
}
