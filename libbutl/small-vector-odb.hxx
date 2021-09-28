// file      : libbutl/small-vector-odb.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <odb/pre.hxx>

#include <libbutl/small-vector.hxx>

#include <odb/container-traits.hxx>

namespace odb
{
  template <typename V, std::size_t N>
  class access::container_traits<butl::small_vector<V, N>>
  {
  public:
    static const container_kind kind = ck_ordered;
    static const bool smart = false;

    using container_type = butl::small_vector<V, N>;

    using value_type = V;
    using index_type = typename container_type::size_type;

    using functions = ordered_functions<index_type, value_type>;

  public:
    static void
    persist (const container_type& c, const functions& f)
    {
      for (index_type i (0), n (c.size ()); i < n; ++i)
        f.insert (i, c[i]);
    }

    static void
    load (container_type& c, bool more, const functions& f)
    {
      c.clear ();

      while (more)
      {
        index_type dummy;
        c.push_back (value_type ());
        more = f.select (dummy, c.back ());
      }
    }

    static void
    update (const container_type& c, const functions& f)
    {
      f.delete_ ();

      for (index_type i (0), n (c.size ()); i < n; ++i)
        f.insert (i, c[i]);
    }

    static void
    erase (const functions& f)
    {
      f.delete_ ();
    }
  };
}

#include <odb/post.hxx>
