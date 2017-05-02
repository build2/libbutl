// file      : libbutl/small-vector.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_SMALL_VECTOR_HXX
#define LIBBUTL_SMALL_VECTOR_HXX

#include <vector>
#include <cassert>
#include <cstddef>     // size_t
#include <utility>     // more(), forward()
#include <type_traits> // true_type

namespace butl
{
  template <typename T, std::size_t N>
  struct small_vector_buffer
  {
    // Size keeps track of the number of elements that are constructed in
    // the buffer. Size equal N + 1 means the buffer is not allocated.
    //
    // Note that the names are decorated in order no to conflict with
    // std::vector interface.
    //
    alignas (alignof (T)) char data_[sizeof (T) * N];
    bool free_ = true;

    // Note that the buffer should be constructed before std::vector and
    // destroyed after (since std::vector's destructor will be destroying
    // elements potentially residing in the buffer). This means that the
    // buffer should be inherited from and before std::vector.
    //
    small_vector_buffer () = default;

    small_vector_buffer (small_vector_buffer&&) = delete;
    small_vector_buffer (const small_vector_buffer&) = delete;

    small_vector_buffer& operator= (small_vector_buffer&&) = delete;
    small_vector_buffer& operator= (const small_vector_buffer&) = delete;
  };

  template <typename T, std::size_t N>
  class small_vector_allocator
  {
  public:
    using buffer_type = small_vector_buffer<T, N>;

    explicit
    small_vector_allocator (buffer_type* b) noexcept: buf_ (b) {}

    // Allocator interface.
    //
  public:
    using value_type = T;

    T*
    allocate(std::size_t n)
    {
      assert (n >= N); // We should never be asked for less than N.

      if (n <= N)
      {
        buf_->free_ = false;
        return reinterpret_cast<T*> (buf_->data_);
      }
      else
        return static_cast<T*> (::operator new (sizeof (T) * n));
    }

    void
    deallocate (void* p, std::size_t) noexcept
    {
      if (p == buf_->data_)
        buf_->free_ = true;
      else
        ::operator delete (p);
    }

    friend bool
    operator== (small_vector_allocator x, small_vector_allocator y) noexcept
    {
      // We can use y to deallocate x's allocations if they use the same small
      // buffer or neither uses its small buffer (which means all allocations,
      // if any, have been from the shared heap). Of course this assumes no
      // copy will be called to deallocate what has been allocated after said
      // copy was made:
      //
      // A x;
      // A y (x);
      // p = x.allocate ();
      // y.deallocate (p);  // Ouch.
      //
      return (x.buf_ == y.buf_) || (x.buf_->free_ && y.buf_->free_);
    }

    friend bool
    operator!= (small_vector_allocator x, small_vector_allocator y) noexcept
    {
      return !(x == y);
    }

    // It might get instantiated but should not be called.
    //
    small_vector_allocator
    select_on_container_copy_construction () const noexcept
    {
      return small_vector_allocator (nullptr);
    }

    // propagate_on_container_copy_assignment = false
    // propagate_on_container_move_assignment = false

    // Swap is not supported (see explanation in small_vector::swap()).
    //
    using propagate_on_container_swap = std::true_type;

    void
    swap (small_vector_allocator&) = delete;

    // Shouldn't be needed except to satisfy some static_assert's.
    //
    template <typename U>
    struct rebind {using other = small_vector_allocator<U, N>;};

  private:
    buffer_type* buf_;
  };

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
  template <typename T, std::size_t N>
  class small_vector: private small_vector_buffer<T, N>,
                      public std::vector<T, small_vector_allocator<T, N>>
  {
  public:
    using allocator_type = small_vector_allocator<T, N>;
    using buffer_type = small_vector_buffer<T, N>;
    using base_type = std::vector<T, small_vector_allocator<T, N>>;

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

    small_vector (small_vector&& v)
      : base_type (allocator_type (this))
    {
      if (v.size () <= N)
        reserve ();

      *this = std::move (v); // Delegate to operator=(&&).
    }

    small_vector&
    operator= (small_vector&& v)
    {
      // VC's implementation of operator=(&&) (both 14 and 15) frees the
      // memory and then reallocated with capacity equal to v.size(). This is
      // clearly sub-optimal (the existing buffer could be reused) so we hope
      // this will be fixed eventually (VSO#367146; reportedly fixed for
      // VC15U1).
      //
#if defined(_MSC_VER) && _MSC_VER <= 1910
      if (v.size () < N)
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

#endif // LIBBUTL_SMALL_VECTOR_HXX