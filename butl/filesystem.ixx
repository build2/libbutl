// file      : butl/filesystem.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2015 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline rmdir_status
  try_rmdir_r (const dir_path& p)
  {
    bool e (dir_exists (p)); //@@ What if it exists but is not a directory?

    if (e)
      rmdir_r (p);

    return e ? rmdir_status::success : rmdir_status::not_exist;
  }

  // permissions
  //
  inline permissions operator& (permissions x, permissions y) {return x &= y;}
  inline permissions operator| (permissions x, permissions y) {return x &= y;}
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
  type () const
  {
    entry_type t (ltype ());
    return t != entry_type::symlink
      ? t
      : lt_ != entry_type::unknown ? lt_ : (lt_ = type (true));
  }

  inline entry_type dir_entry::
  ltype () const
  {
    return t_ != entry_type::unknown ? t_ : (t_ = type (false));
  }

  // dir_iterator
  //
  inline dir_iterator::
  dir_iterator (dir_iterator&& x): e_ (std::move (x.e_)), h_ (x.h_)
  {
    x.h_ = nullptr;
  }

  inline dir_iterator& dir_iterator::
  operator= (dir_iterator&& x)
  {
    if (this != &x)
    {
      e_ = std::move (x.e_);
      h_ = x.h_;
      x.h_ = nullptr;
    }
    return *this;
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

#ifndef _WIN32
  inline dir_iterator::
  ~dir_iterator ()
  {
    if (h_ != nullptr)
      ::closedir (h_); // Ignore any errors.
  }
#else
#endif
}
