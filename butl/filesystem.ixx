// file      : butl/filesystem.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2015 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  // dir_entry
  //
  inline entry_type dir_entry::
  type () const
  {
    return t_ != entry_type::unknown ? t_ : (t_ = type (false));
  }

  inline entry_type dir_entry::
  ltype () const
  {
    entry_type t (type ());
    return t != entry_type::symlink
      ? t
      : lt_ != entry_type::unknown ? lt_ : (lt_ = type (true));
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
