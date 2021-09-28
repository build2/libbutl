// file      : libbutl/multi-index.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <utility>    // declval()
#include <functional> // hash

#include <libbutl/export.hxx>

namespace butl
{
  // Google the "Emulating Boost.MultiIndex with Standard Containers" blog
  // post for details.
  //

  template <typename T>
  struct map_key
  {
    mutable const T* p;

    map_key (const T* v = 0): p (v) {}
    bool operator< (const map_key& x) const {return *p < *x.p;}
    bool operator== (const map_key& x) const {return *p == *x.p;}
  };

  template <typename I>
  struct map_iterator_adapter: I
  {
    typedef const typename I::value_type::second_type value_type;
    typedef value_type* pointer;
    typedef value_type& reference;

    map_iterator_adapter () {}
    map_iterator_adapter (I i): I (i) {}

    map_iterator_adapter&
    operator= (I i) {static_cast<I&> (*this) = i; return *this;}

    reference operator* () const {return I::operator* ().second;}
    pointer operator-> () const {return &I::operator-> ()->second;}
  };
}

namespace std
{
  template <typename T>
  struct hash<butl::map_key<T>>: hash<T>
  {
    size_t
    operator() (butl::map_key<T> x) const
      noexcept (noexcept (declval<hash<T>> () (*x.p)))
    {
      return hash<T>::operator() (*x.p);
    }
  };
}
