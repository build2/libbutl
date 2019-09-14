// file      : libbutl/filesystem.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline bool
  dir_empty (const dir_path& d)
  {
    // @@ Could 0 size be a valid and faster way?
    //
    return dir_iterator (d, false /* ignore_dangling */) == dir_iterator ();
  }

  inline bool
  file_empty (const path& f)
  {
    auto p (path_entry (f));

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

  // auto_rm
  //
  template <typename P>
  inline auto_rm<P>::
  auto_rm (auto_rm&& x)
      : path (std::move (x.path)), active (x.active)
  {
    x.active = false;
  }

  template <typename P>
  inline auto_rm<P>& auto_rm<P>::
  operator= (auto_rm&& x)
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

  // path_match_flags
  //
  inline path_match_flags operator& (path_match_flags x, path_match_flags y)
  {
    return x &= y;
  }

  inline path_match_flags operator| (path_match_flags x, path_match_flags y)
  {
    return x |= y;
  }

  inline path_match_flags operator&= (path_match_flags& x, path_match_flags y)
  {
    return x = static_cast<path_match_flags> (
      static_cast<std::uint16_t> (x) &
      static_cast<std::uint16_t> (y));
  }

  inline path_match_flags operator|= (path_match_flags& x, path_match_flags y)
  {
    return x = static_cast<path_match_flags> (
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
  dir_iterator (dir_iterator&& x) noexcept
    : e_ (std::move (x.e_)), h_ (x.h_), ignore_dangling_ (x.ignore_dangling_)
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

  // path_pattern_iterator
  //
  inline path_pattern_iterator::
  path_pattern_iterator (std::string::const_iterator begin,
                         std::string::const_iterator end)
      : i_ (begin),
        e_ (end)
  {
    next (); // If i_ == e_ we will end up with the end iterator.
  }

  inline path_pattern_iterator::
  path_pattern_iterator (const std::string& s)
      : path_pattern_iterator (s.begin (), s.end ())
  {
  }

  inline bool
  operator== (const path_pattern_iterator& x, const path_pattern_iterator& y)
  {
    return x.t_.has_value () == y.t_.has_value () &&
           (!x.t_ || (x.i_ == y.i_ && x.e_ == y.e_));
  }

  inline bool
  operator!= (const path_pattern_iterator& x, const path_pattern_iterator& y)
  {
    return !(x == y);
  }

  inline path_pattern_iterator
  begin (const path_pattern_iterator& i)
  {
    return i;
  }

  inline path_pattern_iterator
  end (const path_pattern_iterator&)
  {
    return path_pattern_iterator ();
  }

  // patterns
  //
  inline char
  get_literal (const path_pattern_term& t)
  {
    assert (t.literal ());
    return *t.begin;
  }

  inline bool
  path_pattern (const std::string& s)
  {
    for (const path_pattern_term& t: path_pattern_iterator (s))
    {
      if (!t.literal ())
        return true;
    }

    return false;
  }

  // Return true for a pattern containing the specified number of the
  // consecutive star wildcards.
  //
  inline bool
  path_pattern_recursive (const std::string& s, size_t sn)
  {
    std::size_t n (0);
    for (const path_pattern_term& t: path_pattern_iterator (s))
    {
      if (t.star ())
      {
        if (++n == sn)
          return true;
      }
      else
        n = 0;
    }

    return false;
  }

  inline bool
  path_pattern_recursive (const std::string& s)
  {
    return path_pattern_recursive (s, 2);
  }

  inline bool
  path_pattern_self_matching (const std::string& s)
  {
    return path_pattern_recursive (s, 3);
  }

  inline bool
  path_pattern (const path& p)
  {
    for (auto i (p.begin ()); i != p.end (); ++i)
    {
      if (path_pattern (*i))
        return true;
    }

    return false;
  }

  inline size_t
  path_pattern_recursive (const path& p)
  {
    std::size_t r (0);
    for (auto i (p.begin ()); i != p.end (); ++i)
    {
      if (path_pattern_recursive (*i))
        ++r;
    }

    return r;
  }

  inline bool
  path_pattern_self_matching (const path& p)
  {
    return !p.empty () && path_pattern_self_matching (*p.begin ());
  }
}
