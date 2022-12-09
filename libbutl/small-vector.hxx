// file      : libbutl/small-vector.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <vector>
#include <cstddef>     // size_t
#include <utility>     // move()
#include <type_traits> // is_nothrow_move_constructible

#include <libbutl/small-allocator.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // Issues and limitations.
  //
  // - vector::reserve() may allocate more per the spec. But the three main
  //   C++ runtimes (libstdc++, libc++, and msvc) all seem to do the right
  //   thing.
  //
  // - What if in most cases the vector is empty? How can we avoid initial
  //   reserve? Provide no_reserve flag or some such? Is it really worth it?
  //
  // - swap() is deleted (see notes below).
  //
  // - In contrast to std::vector, the references, pointers, and iterators
  //   referring to elements are invalidated after moving from it.
  //
  template <typename T, std::size_t N>
  class small_vector: private small_allocator_buffer<T, N>,
                      public std::vector<T, small_allocator<T, N>>
  {
  public:
    static constexpr const std::size_t small_size = N;

    using buffer_type = small_allocator_buffer<T, N>;
    using allocator_type = small_allocator<T, N>;
    using base_type = std::vector<T, allocator_type>;

    small_vector ()
      : base_type (allocator_type (this))
    {
      reserve ();
    }

    small_vector (std::initializer_list<T> v)
      : base_type (allocator_type (this))
    {
      if (v.size () <= N)
        reserve ();

      static_cast<base_type&> (*this) = v;
    }

    template <typename I>
    small_vector (I b, I e)
      : base_type (allocator_type (this))
    {
      // While we could optimize this for random access iterators, N will
      // usually be pretty small. Let's hope the compiler sees this and does
      // some magic for us.
      //
      std::size_t n (0);
      for (I i (b); i != e && n <= N; ++i) ++n;

      if (n <= N)
        reserve ();

      this->assign (b, e);
    }

    explicit
    small_vector (std::size_t n)
      : base_type (allocator_type (this))
    {
      if (n <= N)
        reserve ();

      this->resize (n);
    }

    small_vector (std::size_t n, const T& x)
      : base_type (allocator_type (this))
    {
      if (n <= N)
        reserve ();

      this->assign (n, x);
    }

    small_vector (const small_vector& v)
      : buffer_type (), base_type (allocator_type (this))
    {
      if (v.size () <= N)
        reserve ();

      static_cast<base_type&> (*this) = v;
    }

    small_vector&
    operator= (const small_vector& v)
    {
      // Note: propagate_on_container_copy_assignment = false
      //
      static_cast<base_type&> (*this) = v;
      return *this;
    }

    // Note that while the move constructor is implemented via the move
    // assignment it may not throw if the value type is no-throw move
    // constructible.
    //
    // Specifically, if v.size() > N then allocators evaluate as equal and the
    // buffer ownership is transferred. Otherwise, the allocators do not
    // evaluate as equal and the individual elements are move-constructed in
    // the preallocated buffer.
    //
    // Also note that this constructor ends up calling
    // base_type::operator=(base_type&&) whose noexcept expression evaluates
    // to false (propagate_on_container_move_assignment and is_always_equal
    // are false for small_allocator; see std::vector documentation for
    // details). We, however, assume that the noexcept expression we use here
    // is strict enough for all "sane" std::vector implementations since
    // small_allocator never throws directly.
    //
    small_vector (small_vector&& v)
      noexcept (std::is_nothrow_move_constructible<T>::value)
      : base_type (allocator_type (this))
    {
      if (v.size () <= N)
        reserve ();

      *this = std::move (v); // Delegate to operator=(&&).

      // Note that in contrast to the move assignment operator, the
      // constructor must clear the other vector.
      //
      v.clear ();
    }

    // Note that when size() <= N and v.size() > N, then allocators of this
    // and other containers do not evaluate as equal. Thus, the memory for the
    // new elements is allocated on the heap and so std::bad_alloc can be
    // thrown. @@ TODO: maybe we could re-implement this case in terms of
    // swap()?
    //
    small_vector&
    operator= (small_vector&& v) noexcept (false)
    {
      // VC's implementation of operator=(&&) (both 14 and 15) frees the
      // memory and then reallocated with capacity equal to v.size(). This is
      // clearly sub-optimal (the existing buffer could be reused) so we hope
      // this will be fixed eventually (VSO#367146; reportedly fixed for
      // VC15U1).
      //
#if defined(_MSC_VER) && _MSC_VER <= 1910
      if (v.size () <= N)
      {
        clear ();
        for (T& x: v)
          push_back (std::move (x));
        v.clear ();
      }
      else
#endif

      // Note: propagate_on_container_move_assignment = false
      //
      static_cast<base_type&> (*this) = std::move (v);

      return *this;
    }

    small_vector&
    operator= (std::initializer_list<T> v)
    {
      static_cast<base_type&> (*this) = v;
      return *this;
    }

    // Implementing swap() under small buffer optimization is not trivial, to
    // say the least (think of swapping two such buffers of different sizes).
    // One easy option would be to force both in to the heap.
    //
    void
    swap (small_vector&) = delete;

    void
    reserve (std::size_t n = N)
    {
      base_type::reserve (n < N ? N : n);
    }

    void
    shrink_to_fit ()
    {
      if (this->capacity () > N)
        base_type::shrink_to_fit ();
    }
  };
}
