// file      : libbutl/path-map.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <algorithm> // min()

#include <libbutl/path.hxx>
#include <libbutl/prefix-map.hxx>

#include <libbutl/export.hxx>

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
    typedef typename key_type::traits_type traits_type;

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

    bool
    prefix (key_type& k) const
    {
      if (k.empty ())
        return false;

      k.make_directory ();
      return true;
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
      return p.size () == 1 && key_type::traits_type::is_separator (p[0]);
    }
  };

  // Note that the delimiter character is not used (is_delimiter() from
  // path_traits is used instead).
  //
  template <typename T>
  using path_map =
    prefix_map<path, T, path::traits_type::directory_separator>;

  template <typename T>
  using dir_path_map =
    prefix_map<dir_path, T, dir_path::traits_type::directory_separator>;

  template <typename T>
  using path_multimap =
    prefix_multimap<path, T, path::traits_type::directory_separator>;

  template <typename T>
  using dir_path_multimap =
    prefix_multimap<dir_path, T, dir_path::traits_type::directory_separator>;
}
