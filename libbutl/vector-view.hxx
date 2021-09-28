// file      : libbutl/vector-view.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <vector>
#include <cstddef>   // size_t, ptrdiff_t
#include <utility>   // swap()
#include <iterator>  // reverse_iterator
#include <stdexcept> // out_of_range

#include <libbutl/export.hxx>

namespace butl
{
  // In our version a const view allows the modification of the elements
  // unless T is made const (the same semantics as in smart pointers).
  //
  // @@ If T is const T1, could be useful to have a c-tor from vector<T1>.
  //
  template <typename T>
  class vector_view
  {
  public:
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    using const_pointer = const T*;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    // construct/copy/destroy:
    //
    vector_view (): data_ (nullptr), size_ (0) {}
    vector_view (T* d, size_type s): data_ (d), size_ (s) {}

    template <typename T1, typename A>
    vector_view (std::vector<T1, A>& v)
        : data_ (v.data ()), size_ (v.size ()) {}

    template <typename T1, typename A>
    vector_view (const std::vector<T1, A>& v)
        : data_ (v.data ()), size_ (v.size ()) {}

    template <typename T1>
    vector_view (const vector_view<T1>& v)
        : data_ (v.data ()), size_ (v.size ()) {}

    vector_view (vector_view&&) = default;
    vector_view (const vector_view&) = default;
    vector_view& operator= (vector_view&&) = default;
    vector_view& operator= (const vector_view&) = default;

    // iterators:
    //
    iterator               begin() const {return data_;}
    iterator               end() const {return data_ + size_;}

    const_iterator         cbegin() const {return data_;}
    const_iterator         cend() const {return data_ + size_;}

    reverse_iterator       rbegin() const {return reverse_iterator (end ());}
    reverse_iterator       rend() const {return reverse_iterator (begin ());}

    const_reverse_iterator crbegin() const {
      return const_reverse_iterator (cend ());}
    const_reverse_iterator crend() const {
      return const_reverse_iterator (cbegin ());}

    // capacity:
    //
    size_type size() const {return size_;}
    bool      empty() const {return size_ == 0;}

    // element access:
    //
    reference  operator[](size_type n) const {return data_[n];}
    reference  front() const {return data_[0];}
    reference  back() const {return data_[size_ - 1];}

    reference  at(size_type n) const
    {
      if (n >= size_)
        throw std::out_of_range ("index out of range");
      return data_[n];
    }

    // data access:
    //
    T* data() const {return data_;}

    // modifiers:
    //
    void assign (T* d, size_type s) {data_ = d; size_ = s;}
    void clear () {data_ = nullptr; size_ = 0;}
    void swap (vector_view& v) {
      std::swap (data_, v.data_); std::swap (size_, v.size_);}

  private:
    T* data_;
    size_type size_;
  };

  //@@ TODO.
  //
  template<typename T> bool operator== (vector_view<T> l, vector_view<T> r);
  template<typename T> bool operator!= (vector_view<T> l, vector_view<T> r);
  template<typename T> bool operator<  (vector_view<T> l, vector_view<T> r);
  template<typename T> bool operator>  (vector_view<T> l, vector_view<T> r);
  template<typename T> bool operator<= (vector_view<T> l, vector_view<T> r);
  template<typename T> bool operator>= (vector_view<T> l, vector_view<T> r);
}
