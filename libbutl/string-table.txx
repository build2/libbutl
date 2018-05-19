// file      : libbutl/string-table.txx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  template <typename I, typename D>
  I string_table<I, D>::
  insert (const D& d)
  {
    std::size_t i (vec_.size () + 1);

    // Note: move(d) would be tricky since the key still points to it.
    //
    auto r (map_.emplace (
              key_type (&traits::key (d)),
              value_type {static_cast<I> (i), d}));

    if (r.second)
    {
      assert (i <= std::numeric_limits<I>::max ());

      r.first->first.p = &traits::key (r.first->second.d); // Update key.
      vec_.push_back (r.first);
    }

    return r.first->second.i;
  }
}
