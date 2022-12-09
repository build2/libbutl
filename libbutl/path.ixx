// file      : libbutl/path.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  // path_abnormality
  //

  inline path_abnormality
  operator& (path_abnormality x, path_abnormality y)
  {
    return x &= y;
  }

  inline path_abnormality
  operator| (path_abnormality x, path_abnormality y)
  {
    return x |= y;
  }

  inline path_abnormality
  operator&= (path_abnormality& x, path_abnormality y)
  {
    return x = static_cast<path_abnormality> (
      static_cast<std::uint16_t> (x) &
      static_cast<std::uint16_t> (y));
  }

  inline path_abnormality
  operator|= (path_abnormality& x, path_abnormality y)
  {
    return x = static_cast<path_abnormality> (
      static_cast<std::uint16_t> (x) |
      static_cast<std::uint16_t> (y));
  }

  // path_traits
  //
  template <typename C>
  inline bool path_traits<C>::
  normalized (const C* s, size_type n, bool sep)
  {
    // An early-return version of abnormalities().
    //
    size_t j (0); // Beginning of path component.

    for (size_t i (0); i != n; ++i)
    {
      char c (s[i]);

      if (is_separator (c))
      {
        if (sep && c != directory_separator)
          return false;

        const char* p (s + j);
        size_t m (i - j);
        j = i + 1;

        if (j != n && is_separator (s[j]))
          return false;

        if (parent (p, m) || current (p, m))
          return false;
      }
    }

    // Last component.
    //
    const char* p (s + j);
    size_t m (n - j);

    return !(parent (p, m) || current (p, m));
  }

  template <typename C>
  inline path_abnormality path_traits<C>::
  abnormalities (const C* s, size_type n)
  {
    path_abnormality r (path_abnormality::none);

    size_t j (0); // Beginning of path component.

    for (size_t i (0); i != n; ++i)
    {
      char c (s[i]);

      if (is_separator (c))
      {
        if (c != directory_separator)
          r |= path_abnormality::separator;

        const char* p (s + j);
        size_t m (i - j);
        j = i + 1;

        if (j != n && is_separator (s[j]))
          r |= path_abnormality::separator;

        if (parent (p, m))
          r |= path_abnormality::parent;
        else if (current (p, m))
          r |= path_abnormality::current;
      }
    }

    // Last component.
    //
    const char* p (s + j);
    size_t m (n - j);

    if (parent (p, m))
      r |= path_abnormality::parent;
    else if (current (p, m))
      r |= path_abnormality::current;

    return r;
  }

  template <typename C>
  inline bool path_traits<C>::
  sub (const C* s, size_type n,
       const C* ps, size_type pn)
  {
    // The thinking here is that we can use the full string representations
    // (including the trailing slash in "/").
    //
    if (pn == 0)
      return true;

    // The second condition guards against the /foo-bar vs /foo case.
    //
    return n >= pn &&
      compare (s, pn, ps, pn) == 0 &&
      (is_separator (ps[pn - 1]) || // p ends with a separator
       n == pn                   || // *this == p
       is_separator (s[pn]));       // next char is a separator
  }

  template <typename C>
  inline bool path_traits<C>::
  sup (const C* s, size_type n,
       const C* ps, size_type pn)
  {
    // The thinking here is that we can use the full string representations
    // (including the trailing slash in "/").
    //
    if (pn == 0)
      return true;

    // The second condition guards against the /foo-bar vs bar case.
    //
    return n >= pn &&
      compare (s + n - pn, pn, ps, pn) == 0 &&
      (n == pn ||                     // *this == p
       is_separator (s[n - pn - 1])); // Previous char is a separator.
  }

#ifdef _WIN32
  template <>
  inline char path_traits<char>::
  tolower (char c)
  {
    return lcase (c);
  }

  template <>
  inline char path_traits<char>::
  toupper (char c)
  {
    return ucase (c);
  }
#endif

  // path
  //

  template <class C, class K1, class K2>
  inline basic_path<C, K1>
  path_cast_impl (const basic_path<C, K2>& p, basic_path<C, K1>*)
  {
    typename basic_path<C, K1>::data_type d (
      typename basic_path<C, K1>::string_type (p.path_), p.tsep_);
    K1::cast (d);
    return basic_path<C, K1> (std::move (d));
  }

  template <class C, class K1, class K2>
  inline basic_path<C, K1>
  path_cast_impl (basic_path<C, K2>&& p, basic_path<C, K1>*)
  {
    typename basic_path<C, K1>::data_type d (std::move (p.path_), p.tsep_);
    K1::cast (d);
    return basic_path<C, K1> (std::move (d));
  }

  template <class P, class C, class K>
  inline P
  path_cast (const basic_path<C, K>& p)
  {
    return path_cast_impl (p, static_cast<P*> (nullptr));
  }

  template <class P, class C, class K>
  inline P
  path_cast (basic_path<C, K>&& p)
  {
    return path_cast_impl (std::move (p), static_cast<P*> (nullptr));
  }

  template <typename C, typename K>
  inline bool basic_path<C, K>::
  simple () const
  {
    return empty () ||
      traits_type::rfind_separator (this->path_, _size () - 1) ==
      string_type::npos;
  }

  template <typename C, typename K>
  inline bool basic_path<C, K>::
  absolute () const
  {
    return traits_type::absolute (this->path_);
  }

  template <typename C, typename K>
  inline bool basic_path<C, K>::
  current () const
  {
    return traits_type::current (this->path_);
  }

  template <typename C, typename K>
  inline bool basic_path<C, K>::
  parent () const
  {
    return traits_type::parent (this->path_);
  }

  template <typename C, typename K>
  inline bool basic_path<C, K>::
  normalized (bool sep) const
  {
    return (!sep || this->tsep_ <= 1) &&
      traits_type::normalized (this->path_, sep);
  }

  template <typename C, typename K>
  inline path_abnormality basic_path<C, K>::
  abnormalities () const
  {
    path_abnormality r (traits_type::abnormalities (this->path_));

    if (this->tsep_ > 1)
      r |= path_abnormality::separator;

    return r;
  }

  template <typename C, typename K>
  inline bool basic_path<C, K>::
  root () const
  {
    return traits_type::root (this->path_);
  }

  template <typename C, typename K>
  inline bool basic_path<C, K>::
  sub (const basic_path& p) const
  {
    return traits_type::sub (this->path_.c_str (), this->path_.size (),
                             p.path_.c_str (), p.path_.size ());
  }

  template <typename C, typename K>
  inline bool basic_path<C, K>::
  sup (const basic_path& p) const
  {
    return traits_type::sup (this->path_.c_str (), this->path_.size (),
                             p.path_.c_str (), p.path_.size ());
  }

  template <typename C, typename K>
  inline basic_path<C, K> basic_path<C, K>::
  leaf () const
  {
    // While it would have been simpler to implement this one in term of
    // make_leaf(), this implementation is potentially more efficient
    // (think of the small string optimization).
    //
    const string_type& s (this->path_);
    size_type n (_size ());

    size_type p (n != 0
                 ? traits_type::rfind_separator (s, n - 1)
                 : string_type::npos);

    return p != string_type::npos
      ? basic_path (data_type (string_type (s, p + 1), this->tsep_))
      : *this;
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  make_leaf ()
  {
    string_type& s (this->path_);
    size_type n (_size ());

    size_type p (n != 0
                 ? traits_type::rfind_separator (s, n - 1)
                 : string_type::npos);

    if (p != string_type::npos)
    {
      s.erase (0, p + 1);

      // Keep the original tsep unless the path became empty.
      //
      if (s.empty ())
        this->tsep_ = 0;
    }

    return *this;
  }

  template <typename C, typename K>
  inline typename basic_path<C, K>::dir_type basic_path<C, K>::
  directory () const
  {
    // While it would have been simpler to implement this one in term of
    // make_directory(), this implementation is potentially more efficient
    // (think of the small string optimization).
    //
    const string_type& s (this->path_);
    size_type n (_size ());

    size_type p (n != 0
                 ? traits_type::rfind_separator (s, n - 1)
                 : string_type::npos);

    return p != string_type::npos
      ? dir_type (data_type (string_type (s, 0, p + 1))) // Include slash.
      : dir_type ();
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  make_directory ()
  {
    string_type& s (this->path_);
    size_type n (_size ());

    size_type p (n != 0
                 ? traits_type::rfind_separator (s, n - 1)
                 : string_type::npos);

    s.resize (p != string_type::npos ? p + 1 : 0); // Include trailing slash.
    _init ();

    return *this;
  }

  template <typename C, typename K>
  inline auto basic_path<C, K>::
  begin () const -> iterator
  {
    const string_type& s (this->path_);

    size_type b (s.empty () ? string_type::npos : 0);
    size_type e (b == 0 ? traits_type::find_separator (s) : b);

    return iterator (this, b, e);
  }

  template <typename C, typename K>
  inline auto basic_path<C, K>::
  end () const -> iterator
  {
    return iterator (this, string_type::npos, string_type::npos);
  }

  template <typename C, typename K>
  inline basic_path<C, K>::
  basic_path (const iterator& b, const iterator& e)
      : base_type (
        b == e
        ? data_type ()
        // We need to include the trailing separator but it is implied if
        // e == end().
        //
        : (e.b_ != string_type::npos
           ? data_type (string_type (b.p_->path_, b.b_, e.b_ - b.b_))
           : data_type (string_type (b.p_->path_, b.b_), b.p_->tsep_)))
  {
    //assert (b.p_ == e.p_);
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  canonicalize (char ds)
  {
    traits_type::canonicalize (this->path_, ds);

    // Canonicalize the trailing separator if any.
    //
    if (this->tsep_ > 0)
    {
      auto dss (traits_type::directory_separators);
      difference_type i (ds == '\0' ? 1 : strchr (dss, ds) - dss + 1);

      if (this->tsep_ != i)
        this->tsep_ = i;
    }

    return *this;
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  complete ()
  {
    if (relative ())
      *this = current_directory () / *this;

    return *this;
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  realize ()
  {
#ifdef _WIN32
    // This is not exactly the semantics of realpath(3). In particular, we
    // don't fail if the path does not exist. But we could have seeing that
    // we actualize it.
    //
    complete ();
    normalize (true);
#else
    traits_type::realize (this->path_); // Note: we retain the trailing slash.
#endif
    return *this;
  }

  template <typename C, typename K>
  inline typename basic_path<C, K>::dir_type basic_path<C, K>::
  root_directory () const
  {
#ifdef _WIN32
    // Note: on Windows we may have "c:" but still need to return "c:\".
    //
    const string_type& s (this->path_);

    return absolute ()
      ? dir_type (
        s.size () > 2
        ? data_type (string_type (s, 0, 3))
        : data_type (string_type (s), this->tsep_ != 0 ? this->tsep_ : 1))
      : dir_type ();
#else
    return absolute ()
      ? dir_type (data_type ("/", -1))
      : dir_type ();
#endif

  }

  template <typename C, typename K>
  inline basic_path<C, K> basic_path<C, K>::
  base () const
  {
    // While it would have been simpler to implement this one in term of
    // make_base(), this implementation is potentially more efficient (think
    // of the small string optimization).
    //
    const string_type& s (this->path_);
    size_type p (traits_type::find_extension (s));

    return p != string_type::npos
      ? basic_path (data_type (string_type (s, 0, p), this->tsep_))
      : *this;
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  make_base ()
  {
    string_type& s (this->path_);
    size_type p (traits_type::find_extension (s));

    if (p != string_type::npos)
    {
      s.resize (p);

      // Keep the original tsep unless the path became empty.
      //
      if (s.empty ())
        this->tsep_ = 0;
    }

    return *this;
  }

  template <typename C, typename K>
  inline typename basic_path<C, K>::string_type basic_path<C, K>::
  extension () const
  {
    const string_type& s (this->path_);
    size_type p (traits_type::find_extension (s));
    return p != string_type::npos
      ? string_type (s.c_str () + p + 1)
      : string_type ();
  }

  template <typename C, typename K>
  inline const C* basic_path<C, K>::
  extension_cstring () const
  {
    const string_type& s (this->path_);
    size_type p (traits_type::find_extension (s));
    return p != string_type::npos ? s.c_str () + p + 1 : nullptr;
  }

#ifndef _WIN32
  template <typename C, typename K>
  inline typename basic_path<C, K>::string_type basic_path<C, K>::
  posix_string () const&
  {
    return string ();
  }

  template <typename C, typename K>
  inline typename basic_path<C, K>::string_type basic_path<C, K>::
  posix_string () &&
  {
    return std::move (*this).string ();
  }

  template <typename C, typename K>
  inline typename basic_path<C, K>::string_type basic_path<C, K>::
  posix_representation () const&
  {
    return representation ();
  }

  template <typename C, typename K>
  inline typename basic_path<C, K>::string_type basic_path<C, K>::
  posix_representation () &&
  {
    return std::move (*this).representation ();
  }
#endif

  template <typename C, typename K>
  inline void basic_path<C, K>::
  combine_impl (const C* r, size_type rn, difference_type rts)
  {
    //assert (rn != 0); // Should be ensured by caller.

    string_type& l (this->path_);
    difference_type& ts (this->tsep_);

    // Handle the separator. LHS should be empty or already have one.
    //
    switch (ts)
    {
    case  0: if (!l.empty ()) l += path_traits<C>::directory_separator; break;
    case -1: break; // Already in the string.
    default: l += path_traits<C>::directory_separators[ts - 1];
    }

    l.append (r, rn);
    ts = rts; // New trailing separator from RHS.
  }

  template <typename C, typename K>
  inline void basic_path<C, K>::
  combine_impl (const C* r, size_type rn)
  {
    // If we do (dir_path / path) then we will end up with path. What should
    // we end up if we do (dir_path / "foo") vs (dir_path / "foo/")? We cannot
    // choose at runtime what kind of path to return. One (elaborate) option
    // would be to handle the trailing slash but also call K::cast() so that
    // dir_path gets the canonical trailing slash if one wasn't there.
    //
    // For now we won't allow the slash and will always add the canonical one
    // for dir_path (via cast()). But also see the public combine() functions.
    //
    if (traits_type::find_separator (r, rn) != nullptr)
      throw invalid_basic_path<C> (r, rn);

    combine_impl (r, rn, 0);
    K::cast (*this);
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  operator/= (basic_path<C, K> const& r)
  {
    if (r.absolute () && !empty ()) // Allow ('' / '/foo').
      throw invalid_basic_path<C> (r.path_);

    if (!r.empty ())
      combine_impl (r.path_.c_str (), r.path_.size (), r.tsep_);

    return *this;
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  operator/= (string_type const& r)
  {
    if (size_type rn = r.size ())
      combine_impl (r.c_str (), rn);

    return *this;
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  operator/= (const C* r)
  {
    if (size_type rn = string_type::traits_type::length (r))
      combine_impl (r, rn);

    return *this;
  }

  template <typename C, typename K>
  inline void basic_path<C, K>::
  combine (string_type const& r, C s)
  {
    combine (r.c_str (), r.size (), s);
  }

  template <typename C, typename K>
  inline void basic_path<C, K>::
  combine (const C* r, C s)
  {
    combine (r, string_type::traits_type::length (r), s);
  }

  template <typename C, typename K>
  inline void basic_path<C, K>::
  combine (const C* r, size_type rn, C s)
  {
    if (rn != 0 || s != '\0')
    {
      if (traits_type::find_separator (r, rn) != nullptr)
        throw invalid_basic_path<C> (r, rn);

#ifndef _WIN32
      if (rn == 0 && empty ()) // POSIX root.
      {
        this->path_ += s;
        this->tsep_ = -1;
        return;
      }
#endif

      if (rn != 0)
        combine_impl (r, rn, 0);

      if (s != '\0')
        this->tsep_ = traits_type::separator_index (s);

      K::cast (*this);
    }
  }

  template <typename C, typename K>
  inline void basic_path<C, K>::
  append (const C* r, size_type rn)
  {
    //assert (this->tsep_ != -1); // Append to root?
    this->path_.append (r, rn);
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  operator+= (string_type const& s)
  {
    append (s.c_str (), s.size ());
    return *this;
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  operator+= (const C* s)
  {
    append (s, string_type::traits_type::length (s));
    return *this;
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  operator+= (C c)
  {
    append (&c, 1);
    return *this;
  }

  template <typename C, typename K>
  inline auto basic_path<C, K>::
  representation () const& -> string_type
  {
    string_type r (this->path_);

    if (this->tsep_ > 0)
      r += path_traits<C>::directory_separators[this->tsep_ - 1];

    return r;
  }

  template <typename C, typename K>
  inline auto basic_path<C, K>::
  representation () && -> string_type
  {
    string_type r;
    r.swap (this->path_);

    if (this->tsep_ > 0)
      r += path_traits<C>::directory_separators[this->tsep_ - 1];

    return r;
  }

  template <typename C, typename K>
  inline C basic_path<C, K>::
  separator () const
  {
    return (this->tsep_ ==  0 ? 0 :
            this->tsep_ == -1 ? this->path_[0] :
            path_traits<C>::directory_separators[this->tsep_ - 1]);
  }

  template <typename C, typename K>
  inline auto basic_path<C, K>::
  separator_string () const -> string_type
  {
    C c (separator ());
    return c == 0 ? string_type () : string_type (1, c);
  }

  template <typename C>
  inline void dir_path_kind<C>::
  cast (data_type& d)
  {
    // Add trailing slash if one isn't already there.
    //
    if (!d.path_.empty () && d.tsep_ == 0)
      d.tsep_ = 1; // Canonical separator is always first.
  }

  template <typename C, typename K>
  inline std::basic_ostream<C>&
  to_stream (std::basic_ostream<C>& os,
             const basic_path<C, K>& p,
             bool representation)
  {
    os << p.string ();

    if (representation)
    {
      C sep (p.separator ());

#ifndef _WIN32
      if (sep != 0 && !p.root ())
        os << sep;
#else
      if (sep != 0)
        os << sep;
#endif
    }

    return os;
  }

  // basic_path_name
  //
  template <typename P>
  inline basic_path_name<P>::
  basic_path_name (basic_path_name&& p) noexcept
      : basic_path_name (p.path, std::move (p.name))
  {
  }

  template <typename P>
  inline basic_path_name<P>::
  basic_path_name (const basic_path_name& p)
      : basic_path_name (p.path, p.name)
  {
  }

  template <typename P>
  inline basic_path_name<P>& basic_path_name<P>::
  operator= (basic_path_name&& p) noexcept
  {
    if (this != &p)
    {
      this->path = p.path;
      name = std::move (p.name);
    }

    return *this;
  }

  template <typename P>
  inline basic_path_name<P>& basic_path_name<P>::
  operator= (const basic_path_name& p)
  {
    if (this != &p)
    {
      this->path = p.path;
      name = p.name;
    }

    return *this;
  }

  // basic_path_name_value
  //
  template <typename P>
  inline basic_path_name_value<P>::
  basic_path_name_value (basic_path_name_value&& p) noexcept
      : basic_path_name_value (std::move (p.path), std::move (p.name))
  {
  }

  template <typename P>
  inline basic_path_name_value<P>::
  basic_path_name_value (const basic_path_name_value& p)
      : basic_path_name_value (p.path, p.name)
  {
  }

  template <typename P>
  inline basic_path_name_value<P>& basic_path_name_value<P>::
  operator= (basic_path_name_value&& p) noexcept
  {
    if (this != &p)
    {
      path = std::move (p.path);
      this->name = std::move (p.name);
    }

    return *this;
  }

  template <typename P>
  inline basic_path_name_value<P>& basic_path_name_value<P>::
  operator= (const basic_path_name_value& p)
  {
    if (this != &p)
    {
      path = p.path;
      this->name = p.name;
    }

    return *this;
  }
}
