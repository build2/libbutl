// file      : libbutl/path-pattern.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace butl
{
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
  path_pattern_recursive (const std::string& s, std::size_t sn)
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

  inline std::size_t
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
