// file      : butl/path.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifdef _WIN32
#  include <cctype>  // tolower(), toupper()
#  include <cwctype> // towlower(), towupper()
#endif

namespace butl
{
#ifdef _WIN32
  template <>
  inline char path_traits<char>::
  tolower (char c)
  {
    return std::tolower (c);
  }

  template <>
  inline wchar_t path_traits<wchar_t>::
  tolower (wchar_t c)
  {
    return std::towlower (c);
  }

  template <>
  inline char path_traits<char>::
  toupper (char c)
  {
    return std::toupper (c);
  }

  template <>
  inline wchar_t path_traits<wchar_t>::
  toupper (wchar_t c)
  {
    return std::towupper (c);
  }
#endif

  template <class C, class K1, class K2>
  inline basic_path<C, K1>
  path_cast_impl (const basic_path<C, K2>& p, basic_path<C, K1>*)
  {
    typename basic_path<C, K1>::data_type d (
      typename basic_path<C, K1>::string_type (p.path_), p.diff_);
    K1::cast (d);
    return basic_path<C, K1> (std::move (d));
  }

  template <class C, class K1, class K2>
  inline basic_path<C, K1>
  path_cast_impl (basic_path<C, K2>&& p, basic_path<C, K1>*)
  {
    typename basic_path<C, K1>::data_type d (std::move (p.path_), p.diff_);
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
      traits::rfind_separator (this->path_, _size () - 1) == string_type::npos;
  }

  template <typename C, typename K>
  inline bool basic_path<C, K>::
  absolute () const
  {
    const string_type& s (this->path_);

#ifdef _WIN32
    return s.size () >  1 && s[1] == ':';
#else
    return s.size () != 0 && traits::is_separator (s[0]);
#endif
  }

  template <typename C, typename K>
  inline bool basic_path<C, K>::
  root () const
  {
    const string_type& s (this->path_);

#ifdef _WIN32
    return s.size () == 2 && s[1] == ':';
#else
    return s.size () == 1 && traits::is_separator (s[0]);
#endif
  }

  template <typename C, typename K>
  inline bool basic_path<C, K>::
  sub (const basic_path& p) const
  {
    // The thinking here is that we can use the full string representations
    // (including the trailing slash in "/").
    //
    const string_type& ps (p.path_);
    size_type pn (ps.size ());

    if (pn == 0)
      return true;

    const string_type& s (this->path_);
    size_type n (s.size ());

    // The second condition guards against the /foo-bar vs /foo case.
    //
    return n >= pn &&
      traits::compare (s.c_str (), pn, ps.c_str (), pn) == 0 &&
      (traits::is_separator (ps.back ())      || // p ends with a separator
       n == pn                                || // *this == p
       traits::is_separator (s[pn]));            // next char is a separator
  }

  template <typename C, typename K>
  inline bool basic_path<C, K>::
  sup (const basic_path& p) const
  {
    // The thinking here is that we can use the full string representations
    // (including the trailing slash in "/").
    //
    const string_type& ps (p.path_);
    size_type pn (ps.size ());

    if (pn == 0)
      return true;

    const string_type& s (this->path_);
    size_type n (s.size ());

    // The second condition guards against the /foo-bar vs bar case.
    //
    return n >= pn &&
      traits::compare (s.c_str () +  n - pn, pn, ps.c_str (), pn) == 0 &&
      (n == pn ||                             // *this == p
       traits::is_separator (s[n - pn - 1])); // previous char is a separator
  }

  template <typename C, typename K>
  inline basic_path<C, K> basic_path<C, K>::
  leaf () const
  {
    const string_type& s (this->path_);
    size_type n (_size ());

    size_type p (n != 0
                 ? traits::rfind_separator (s, n - 1)
                 : string_type::npos);

    return p != string_type::npos
      ? basic_path (data_type (string_type (s, p + 1), this->diff_))
      : *this;
  }

  template <typename C, typename K>
  inline typename basic_path<C, K>::dir_type basic_path<C, K>::
  directory () const
  {
    const string_type& s (this->path_);
    size_type n (_size ());

    size_type p (n != 0
                 ? traits::rfind_separator (s, n - 1)
                 : string_type::npos);

    return p != string_type::npos
      ? dir_type (data_type (string_type (s, 0, p + 1))) // Include slash.
      : dir_type ();
  }

  template <typename C, typename K>
  inline auto basic_path<C, K>::
  begin () const -> iterator
  {
    const string_type& s (this->path_);

    size_type b (s.empty () ? string_type::npos : 0);
    size_type e (b == 0 ? traits::find_separator (s) : b);

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
           : data_type (string_type (b.p_->path_, b.b_), b.p_->diff_)))
  {
    //assert (b.p_ == e.p_);
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  complete ()
  {
    if (relative ())
      *this = current () / *this;

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
    traits::realize (this->path_); // Note: we retain the trailing slash.
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
        : data_type (string_type (s), this->diff_ != 0 ? this->diff_ : 1))
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
    const string_type& s (this->path_);
    size_type p (traits::find_extension (s));

    return p != string_type::npos
      ? basic_path (data_type (string_type (s, 0, p), this->diff_))
      : *this;
  }

  template <typename C, typename K>
  inline const C* basic_path<C, K>::
  extension () const
  {
    const string_type& s (this->path_);
    size_type p (traits::find_extension (s));
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
  combine (const C* r, size_type rn, difference_type rd)
  {
    //assert (rn != 0);

    string_type& l (this->path_);
    difference_type& d (this->diff_);

    // Handle the separator. LHS should be empty or already have one.
    //
    switch (d)
    {
    case  0: if (!l.empty ()) throw invalid_basic_path<C> (l); break;
    case -1: break; // Already in the string.
    default: l += path_traits<C>::directory_separators[d - 1];
    }

    l.append (r, rn);
    d = rd; // New trailing separator from RHS.
  }

  template <typename C, typename K>
  inline void basic_path<C, K>::
  combine (const C* r, size_type rn)
  {
    // If we do (dir_path / path) then we will end up with path. What should
    // we end up if we do (dir_path / "foo") vs (dir_path / "foo/")? We cannot
    // choose at runtime what kind of path to return. One (elaborate) option
    // would be to handle the trailing slash but also call K::cast() so that
    // dir_path gets the canonical trailing slash if one wasn't there.
    //
    // For now we won't allow the slash and will always add the canonical one
    // for dir_path (via cast()).
    //
    if (traits::find_separator (r, rn) != nullptr)
      throw invalid_basic_path<C> (r);

    combine (r, rn, 0);
    K::cast (*this);
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  operator/= (basic_path<C, K> const& r)
  {
    if (r.absolute () && !empty ()) // Allow ('' / '/foo').
      throw invalid_basic_path<C> (r.path_);

    if (!r.empty ())
      combine (r.path_.c_str (), r.path_.size (), r.diff_);

    return *this;
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  operator/= (string_type const& r)
  {
    if (size_type rn = r.size ())
      combine (r.c_str (), rn);

    return *this;
  }

  template <typename C, typename K>
  inline basic_path<C, K>& basic_path<C, K>::
  operator/= (const C* r)
  {
    if (size_type rn = string_type::traits_type::length (r))
      combine (r, rn);

    return *this;
  }

  template <typename C, typename K>
  inline void basic_path<C, K>::
  append (const C* r, size_type rn)
  {
    //assert (this->diff_ != -1); // Append to root?
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

    if (this->diff_ > 0)
      r += path_traits<C>::directory_separators[this->diff_ - 1];

    return r;
  }

  template <typename C, typename K>
  inline auto basic_path<C, K>::
  representation () && -> string_type
  {
    string_type r;
    r.swap (this->path_);

    if (this->diff_ > 0)
      r += path_traits<C>::directory_separators[this->diff_ - 1];

    return r;
  }

  template <typename C, typename K>
  inline C basic_path<C, K>::
  separator () const
  {
    return (this->diff_ ==  0 ? 0 :
            this->diff_ == -1 ? this->path_[0] :
            path_traits<C>::directory_separators[this->diff_ - 1]);
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
    if (!d.path_.empty () && d.diff_ == 0)
      d.diff_ = 1; // Canonical separator is always first.
  }
}
