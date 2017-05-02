// file      : libbutl/prefix-map.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_PREFIX_MAP_HXX
#define LIBBUTL_PREFIX_MAP_HXX

#include <map>
#include <string>
#include <utility>   // move()
#include <algorithm> // min()

namespace butl
{
  // A map of hierarchical "paths", e.g., 'foo.bar' or 'foo/bar' with
  // the ability to retrieve a range of entries that have a specific
  // prefix. The '.' and '/' above are the delimiter characters.
  //
  // Note that as a special rule, the default implementation of
  // compare_prefix treats empty key as everyone's prefix even if
  // the paths don't start with the delimiter (useful to represent
  // a "root path").
  //
  // Implementation-wise, the idea is to pretend that each key ends
  // with the delimiter. This way we automatically avoid matching
  // 'foobar' as having a prefix 'foo'.
  //
  template <typename K>
  struct compare_prefix;

  template <typename C>
  struct compare_prefix<std::basic_string<C>>
  {
    typedef std::basic_string<C> K;

    typedef C delimiter_type;
    typedef typename K::size_type size_type;
    typedef typename K::traits_type traits_type;

    explicit
    compare_prefix (delimiter_type d): d_ (d) {}

    bool
    operator() (const K& x, const K& y) const
    {
      return compare (x.c_str (), x.size (), y.c_str (), y.size ()) < 0;
    }

    bool
    prefix (const K& p, const K& k) const
    {
      size_type pn (p.size ()), kn (k.size ());
      return pn == 0 || // Empty key is always a prefix.
        (pn <= kn &&
         compare (p.c_str (), pn, k.c_str (), pn == kn ? pn : pn + 1) == 0);
    }

  protected:
    int
    compare (const C* x, size_type xn,
             const C* y, size_type yn) const
    {
      size_type n (std::min (xn, yn));
      int r (traits_type::compare (x, y, n));

      if (r == 0)
      {
        // Pretend there is the delimiter characters at the end of the
        // shorter string.
        //
        char xc (xn > n ? x[n] : (xn++, d_));
        char yc (yn > n ? y[n] : (yn++, d_));
        r = traits_type::compare (&xc, &yc, 1);

        // If we are still equal, then compare the lengths.
        //
        if (r == 0)
          r = (xn == yn ? 0 : (xn < yn ? -1 : 1));
      }

      return r;
    }

  private:
    delimiter_type d_;
  };

  template <typename M>
  struct prefix_map_common: M
  {
    typedef M map_type;
    typedef typename map_type::key_type key_type;
    typedef typename map_type::value_type value_type;
    typedef typename map_type::key_compare compare_type;
    typedef typename compare_type::delimiter_type delimiter_type;

    typedef typename map_type::iterator iterator;
    typedef typename map_type::const_iterator const_iterator;

    explicit
    prefix_map_common (delimiter_type d)
        : map_type (compare_type (d)) {}

    prefix_map_common (std::initializer_list<value_type> i, delimiter_type d)
        : map_type (std::move (i), compare_type (d)) {}

    std::pair<iterator, iterator>
    find_prefix (const key_type&);

    std::pair<const_iterator, const_iterator>
    find_prefix (const key_type&) const;
  };

  template <typename M, typename prefix_map_common<M>::delimiter_type D>
  struct prefix_map_impl: prefix_map_common<M>
  {
    typedef typename prefix_map_common<M>::value_type value_type;

    prefix_map_impl (): prefix_map_common<M> (D) {}
    prefix_map_impl (std::initializer_list<value_type> i)
        : prefix_map_common<M> (std::move (i), D) {}
  };

  template <typename K,
            typename T,
            typename compare_prefix<K>::delimiter_type D>
  using prefix_map = prefix_map_impl<std::map<K, T, compare_prefix<K>>, D>;

  template <typename K,
            typename T,
            typename compare_prefix<K>::delimiter_type D>
  using prefix_multimap =
    prefix_map_impl<std::multimap<K, T, compare_prefix<K>>, D>;
}

#include <libbutl/prefix-map.txx>

#endif // LIBBUTL_PREFIX_MAP_HXX