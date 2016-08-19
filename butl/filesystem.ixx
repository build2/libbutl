// file      : butl/filesystem.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline bool
  dir_empty (const dir_path& d)
  {
    return dir_iterator (d) == dir_iterator ();
  }

  inline rmdir_status
  try_rmdir_r (const dir_path& p, bool ignore_error)
  {
    bool e (dir_exists (p)); //@@ What if it exists but is not a directory?

    if (e)
      rmdir_r (p, true, ignore_error);

    return e ? rmdir_status::success : rmdir_status::not_exist;
  }

  // auto_rm
  //
  template <typename P>
  inline auto_rm<P>::
  auto_rm (auto_rm&& x)
      : path_ (std::move (x.path_))
  {
    x.cancel ();
  }

  template <typename P>
  inline auto_rm<P>& auto_rm<P>::
  operator= (auto_rm&& x)
  {
    if (this != &x)
    {
      path_ = std::move (x.path_);
      x.cancel ();
    }

    return *this;
  }

  template <>
  inline auto_rm<path>::
  ~auto_rm () {if (!path_.empty ()) try_rmfile (path_, true);}

  template <>
  inline auto_rm<dir_path>::
  ~auto_rm () {if (!path_.empty ()) try_rmdir_r (path_, true);}

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
    return t_ != entry_type::unknown ? t_ : (t_ = type (false));
  }

  inline entry_type dir_entry::
  type () const
  {
    entry_type t (ltype ());
    return t != entry_type::symlink
      ? t
      : lt_ != entry_type::unknown ? lt_ : (lt_ = type (true));
  }

  // dir_iterator
  //
  inline dir_iterator::
  dir_iterator (dir_iterator&& x)
      : e_ (std::move (x.e_)), h_ (x.h_)
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
