// file      : libbutl/path-map.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_PATH_MAP_HXX
#define LIBBUTL_PATH_MAP_HXX

#include <algorithm> // min()

#include <libbutl/path.hxx>
#include <libbutl/prefix-map.hxx>

namespace butl
{
  // prefix_map for filesystem paths
  //
  // Important: the paths should be normalized but can use different directory
  // separators and different case on case-insensitive platforms.
  //
  // Note that the path's representation of POSIX root ('/') is inconsistent
  // in that we have a trailing delimiter at the end of the path (its "proper"
  // representation would have been an empty string but that would have
  // clashed with empty paths). To work around this snag, this implementation,
  // during key comparison, detects '/' and treats it as empty. Note that the
  // map will still store the key as you have first inserted it. So if you
  // want a particular representation (i.e., empty or '/'), pre- populate the
  // map with it.
  //
  template <typename C, typename K>
  struct compare_prefix<basic_path<C, K>>
  {
    typedef basic_path<C, K> key_type;

    typedef C delimiter_type;
    typedef typename key_type::string_type string_type;
    typedef typename key_type::size_type size_type;
    typedef typename key_type::traits traits_type;

    explicit
    compare_prefix (delimiter_type) {}

    bool
    operator() (const key_type& x, const key_type& y) const
    {
      const string_type& xs (x.string ());
      const string_type& ys (y.string ());

      return compare (xs.c_str (),
                      root (xs) ? 0 : xs.size (),
                      ys.c_str (),
                      root (ys) ? 0 : ys.size ()) < 0;
    }

    bool
    prefix (const key_type& p, const key_type& k) const
    {
      const string_type& ps (p.string ());
      const string_type& ks (k.string ());

      return prefix (root (ps) ? string_type () : ps,
                     root (ks) ? string_type () : ks);
    }

  protected:
    bool
    prefix (const string_type& p, const string_type& k) const
    {
      // The same code as in prefix_map but using our compare().
      //
      size_type pn (p.size ()), kn (k.size ());
      return pn == 0 || // Empty key is always a prefix.
        (pn <= kn &&
         compare (p.c_str (), pn, k.c_str (), pn == kn ? pn : pn + 1) == 0);
    }

    int
    compare (const C* x, size_type xn,
             const C* y, size_type yn) const
    {
      size_type n (std::min (xn, yn));
      int r (traits_type::compare (x, n, y, n));

      if (r == 0)
      {
        // Pretend there is a delimiter characters at the end of the
        // shorter string.
        //
        char xc (xn > n ? x[n] : (xn++, traits_type::directory_separator));
        char yc (yn > n ? y[n] : (yn++, traits_type::directory_separator));
        r = traits_type::compare (&xc, 1, &yc, 1);

        // If we are still equal, then compare the lengths.
        //
        if (r == 0)
          r = (xn == yn ? 0 : (xn < yn ? -1 : 1));
      }

      return r;
    }

    static bool
    root (const string_type& p)
    {
      return p.size () == 1 && key_type::traits::is_separator (p[0]);
    }
  };

  // Note that the delimiter character is not used (is_delimiter() from
  // path_traits is used instead).
  //
  template <typename P, typename T>
  struct path_map_impl: prefix_map<P, T, P::traits::directory_separator>
  {
    using base = prefix_map<P, T, P::traits::directory_separator>;
    using base::base;

    using iterator = typename base::iterator;
    using const_iterator = typename base::const_iterator;

    // Find the most qualified entry that is a superpath of this path.
    //
    iterator
    find_sup (const P& p)
    {
      // Get the greatest less than, if any. We might still not be a sub. Note
      // also that we still have to check the last element if upper_bound()
      // returned end().
      //
      auto i (this->upper_bound (p));
      return i == this->begin () || !p.sub ((--i)->first) ? this->end () : i;
    }

    const_iterator
    find_sup (const P& p) const
    {
      auto i (this->upper_bound (p));
      return i == this->begin () || !p.sub ((--i)->first) ? this->end () : i;
    }
  };

  template <typename T>
  using path_map = path_map_impl<path, T>;

  template <typename T>
  using dir_path_map = path_map_impl<dir_path, T>;
}

#endif // LIBBUTL_PATH_MAP_HXX
