// file      : libbutl/path.txx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  template <typename C, typename K>
  basic_path<C, K> basic_path<C, K>::
  leaf (basic_path<C, K> const& d) const
  {
    size_type dn (d.path_.size ());

    if (dn == 0)
      return *this;

    const string_type& s (this->path_);

    if (!sub (d))
      throw invalid_basic_path<C> (s);

    // If there is implied trailing slash, add it to count. Unless it is
    // "matched" by the implied slash on the other side.
    //
    if (d.tsep_ > 0 && dn < s.size ())
      dn++;

    // Preserve trailing slash.
    //
    return basic_path (data_type (string_type (s, dn, s.size () - dn),
                                  this->tsep_));
  }

  template <typename C, typename K>
  typename basic_path<C, K>::dir_type basic_path<C, K>::
  directory (basic_path<C, K> const& l) const
  {
    size_type ln (l.path_.size ());

    const string_type& s (this->path_);

    if (ln == 0)
    {
      if (this->tsep_ == 0) // Must be a directory.
        throw invalid_basic_path<C> (s);

      return dir_type (data_type (string_type (s), this->tsep_));
    }

    if (!sup (l))
      throw invalid_basic_path<C> (s);

    return dir_type (
      data_type (string_type (s, 0, s.size () - ln))); // Include slash.
  }

#ifdef _WIN32
  template <typename C, typename K>
  typename basic_path<C, K>::string_type basic_path<C, K>::
  posix_string () const&
  {
    if (absolute ())
      throw invalid_basic_path<C> (this->path_);

    string_type r (string ());
    replace (r.begin (), r.end (), '\\', '/');
    return r;
  }

  template <typename C, typename K>
  typename basic_path<C, K>::string_type basic_path<C, K>::
  posix_string () &&
  {
    if (absolute ())
      throw invalid_basic_path<C> (this->path_);

    string_type r (std::move (*this).string ());
    replace (r.begin (), r.end (), '\\', '/');
    return r;
  }

  template <typename C, typename K>
  typename basic_path<C, K>::string_type basic_path<C, K>::
  posix_representation () const&
  {
    if (absolute ())
      throw invalid_basic_path<C> (this->path_);

    string_type r (representation ());
    replace (r.begin (), r.end (), '\\', '/');
    return r;
  }

  template <typename C, typename K>
  typename basic_path<C, K>::string_type basic_path<C, K>::
  posix_representation () &&
  {
    if (absolute ())
      throw invalid_basic_path<C> (this->path_);

    string_type r (std::move (*this).representation ());
    replace (r.begin (), r.end (), '\\', '/');
    return r;
  }
#endif

  template <typename C, typename K>
  optional<basic_path<C, K>> basic_path<C, K>::
  try_relative (basic_path<C, K> d) const
  {
    dir_type r;

    for (;; d = d.directory ())
    {
      if (sub (d))
        break;

      r /= "..";

      // Roots of the paths do not match.
      //
      if (d.root ())
        return nullopt;
    }

    return r / leaf (d);
  }

  template <typename C, typename K>
  basic_path<C, K> basic_path<C, K>::
  relative (basic_path<C, K> d) const
  {
    if (optional<basic_path<C, K>> r = try_relative (std::move (d)))
      return std::move (*r);

    throw invalid_basic_path<C> (this->path_);
  }

#ifdef _WIN32
  // Find the actual spelling of a name in the specified dir. If the name is
  // found, append it to the result and return true. Otherwise, return false.
  // Throw system_error in case of other failures. Result and dir can be the
  // same instance.
  //
  template <typename C>
  bool
  basic_path_append_actual_name (std::basic_string<C>& result,
                                 const std::basic_string<C>& dir,
                                 const std::basic_string<C>& name);
#endif

  template <typename C, typename K>
  basic_path<C, K>& basic_path<C, K>::
  normalize (bool actual, bool cur_empty)
  {
    if (empty ())
      return *this;

    bool abs (absolute ());
    assert (!actual || abs); // Only absolue can be actualized.

    string_type& s (this->path_);
    difference_type& ts (this->tsep_);

    using paths = small_vector<string_type, 16>;
    paths ps;

    bool tsep (ts != 0); // Trailing directory separator.
    {
      size_type n (_size ());

      for (size_type b (0), e (traits_type::find_separator (s, 0, n));
           ;
           e = traits_type::find_separator (s, b, n))
      {
        ps.push_back (
          string_type (s, b, (e == string_type::npos ? n : e) - b));

        if (e == string_type::npos)
          break;

        ++e;

        // Skip consecutive directory separators.
        //
        while (e != n && traits_type::is_separator (s[e]))
          ++e;

        if (e == n)
          break;

        b = e;
      }

      // If the last component is "." or ".." then this is a directory.
      //
      if (!tsep)
      {
        const string_type& l (ps.back ());
        if (traits_type::current (l) || traits_type::parent (l))
          tsep = true;
      }
    }

    // Collapse "." and "..".
    //
    paths r;

    for (typename paths::iterator i (ps.begin ()), e (ps.end ()); i != e; ++i)
    {
      string_type& s (*i);

      if (traits_type::current (s))
        continue;

      // If '..' then pop the last directory from r unless it is '..'.
      //
      if (traits_type::parent (s) &&
          !r.empty ()             &&
          !traits_type::parent (r.back ()))
      {
        // Cannot go past the root directory.
        //
        if (abs && r.size () == 1)
          throw invalid_basic_path<C> (this->path_);

        r.pop_back ();
        continue;
      }

      r.push_back (std::move (s));
    }

    // Reassemble the path, actualizing each component if requested.
    //
    string_type p;

    for (typename paths::const_iterator b (r.begin ()), i (b), e (r.end ());
         i != e;)
    {
#ifdef _WIN32
      if (actual)
      {
        if (i == b)
        {
          // The first component (the drive letter) we have to actualize
          // ourselves. Capital seems to be canonical. This is, for example,
          // what getcwd() returns.
          //
          p = *i;
          p[0] = traits_type::toupper (p[0]);
        }
        else
        {
          if (!basic_path_append_actual_name (p, p, *i))
          {
            p += *i;
            actual = false; // Ignore for all subsequent components.
          }
        }
      }
      else
#endif
        p += *i;

      if (++i != e)
        p += traits_type::directory_separator;
    }

    if (tsep)
    {
      if (p.empty ())
      {
        // Distinguish "/"-empty and "."-empty.
        //
        if (abs)
        {
          p += traits_type::directory_separator;
          ts = -1;
        }
        else if (!cur_empty) // Collapse to canonical current directory.
        {
          p.assign (1, '.');
          ts = 1; // Canonical separator is always first.
        }
        else // Collapse to empty path.
          ts = 0;
      }
      else
        ts = 1; // Canonical separator is always first.
    }
    else
      ts = 0;

    s.swap (p);
    return *this;
  }

  template <typename C, typename K>
  void basic_path<C, K>::
  current_directory (basic_path const& p)
  {
    const string_type& s (p.string ());

    if (s.empty ())
      throw invalid_basic_path<char> (s);

    traits_type::current_directory (s);
  }

  template <typename C>
  auto any_path_kind<C>::
  init (string_type&& s, bool exact) -> data_type
  {
    using size_type = typename string_type::size_type;
    using difference_type = typename string_type::difference_type;

    size_type n (s.size ());

#ifdef _WIN32
    // We do not support any special Windows path name notations like in C:abc,
    // /, \, /abc, \abc, \\?\c:\abc, \\server\abc and \\?\UNC\server\abc (more
    // about them in "Naming Files, Paths, and Namespaces" MSDN article).
    //
    if ((n > 2 && s[1] == ':' && s[2] != '\\' && s[2] != '/') ||
        (n > 0 && (s[0] == '\\' || s[0] == '/')))
    {
      if (exact)
        return data_type ();
      else
        throw invalid_basic_path<C> (s);
    }
#endif

    // Strip trailing slashes.
    //
    size_type m (n), di (0);
    for (size_type i;
         m != 0 && (i = path_traits<C>::separator_index (s[m - 1])) != 0;
         --m) di = i;

    difference_type ts (0);
    if (size_t k = n - m)
    {
      // We can only accomodate one trailing slash in the exact mode.
      //
      if (exact && k > 1)
        return data_type ();

      if (m == 0) // The "/" case.
      {
        ++m; // Keep one slash in the string.
        ts = -1;
      }
      else
        ts = di;

      s.resize (m);
    }

    return data_type (std::move (s), ts);
  }

  template <typename C>
  auto dir_path_kind<C>::
  init (string_type&& s, bool exact) -> data_type
  {
    // If we don't already have the separator then this can't be the exact
    // initialization.
    //
    if (exact && !s.empty () && !path_traits<C>::is_separator (s.back ()))
      return data_type ();

    data_type r (any_path_kind<C>::init (std::move (s), exact));

    // Unless the result is empty, make sure we have the trailing slash.
    //
    if (!r.path_.empty () && r.tsep_ == 0)
      r.tsep_ = 1; // Canonical separator is always first.

    return r;
  }
}
