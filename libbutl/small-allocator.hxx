// file      : libbutl/small-allocator.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <cassert>
#include <cstddef>     // size_t
#include <utility>     // move()
#include <type_traits> // true_type, is_same

#include <libbutl/export.hxx>

namespace butl
{
  // Implementation of the allocator (and its buffer) for small containers.
  //
  template <typename T, std::size_t N>
  struct small_allocator_buffer
  {
    using value_type = T;

    // Note that the names are decorated in order not to conflict with
    // the container's interface.

    // If free_ is true then the buffer is not allocated.
    //
    alignas (alignof (value_type)) char data_[sizeof (value_type) * N];
    bool free_ = true;

    // Note that the buffer should be constructed before the container and
    // destroyed after (since the container's destructor will be destroying
    // elements potentially residing in the buffer). This means that the
    // buffer should be inherited from and before the std:: container.
    //
    small_allocator_buffer () = default;

    small_allocator_buffer (small_allocator_buffer&&) = delete;
    small_allocator_buffer (const small_allocator_buffer&) = delete;

    small_allocator_buffer& operator= (small_allocator_buffer&&) = delete;
    small_allocator_buffer& operator= (const small_allocator_buffer&) = delete;
  };

  template <typename T,
            std::size_t N,
            typename B = small_allocator_buffer<T, N>>
  class small_allocator
  {
  public:
    using buffer_type = B;

    explicit
    small_allocator (buffer_type* b) noexcept: buf_ (b) {}

    // Allocator interface.
    //
  public:
    using value_type      = T;

    // These shouldn't be required but as usual there are old/broken
    // implementations (like std::list in GCC 4.9).
    //
    using pointer         =       value_type*;
    using const_pointer   = const value_type*;
    using reference       =       value_type&;
    using const_reference = const value_type&;

    static void destroy (T* p) {p->~T ();}

    template <typename... A>
    static void construct (T* p, A&&... a)
    {
      ::new (static_cast<void*> (p)) T (std::forward<A> (a)...);
    }

    // Allocator rebinding.
    //
    // We assume that only one of the rebound allocators will actually be
    // doing allocations and that its value type is the same as buffer value
    // type. This is needed, for instance, for std::list since what actually
    // gets allocated is the node type, not T (see small_list for details).
    //
    template <typename U>
    struct rebind {using other = small_allocator<U, N, B>;};

    template <typename U>
    explicit
    small_allocator (const small_allocator<U, N, B>& x) noexcept
      : buf_ (x.buf_) {}

    T*
    allocate (std::size_t n)
    {
      // An implementation can rebind the allocator to something completely
      // different. For example, VC15u3 with _ITERATOR_DEBUG_LEVEL != 0
      // allocates some extra stuff which cannot possibly come from the static
      // buffer.
      //
      if (std::is_same<T, typename buffer_type::value_type>::value)
      {
        if (buf_->free_)
        {
          assert (n >= N); // We should never be asked for less than N.

          if (n == N)
          {
            buf_->free_ = false;
            return reinterpret_cast<T*> (buf_->data_);
          }
          // Fall through.
        }
      }

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
    operator== (small_allocator x, small_allocator y) noexcept
    {
      // We can use y to deallocate x's allocations if they use the same small
      // buffer or neither uses its small buffer (which means all allocations,
      // if any, have been from the shared heap).
      //
      // Things get trickier with rebinding. If A is allocator and B is its
      // rebinding, then the following must hold true:
      //
      // A a1(a)  =>  a1==a
      // A a(b)   =>  B(a)==b && A(b)==a
      //
      // As a result, the rebinding constructor above always copies the buffer
      // pointer and we decide whether to use the small buffer by comparing
      // allocator/buffer value types.
      //
      // We also expect that any copy of the original allocator made by the
      // std:: implementation of the container is temporary (that is, it
      // doesn't outlive the small buffer).
      //
      return (x.buf_ == y.buf_) || (x.buf_->free_ && y.buf_->free_);
    }

    friend bool
    operator!= (small_allocator x, small_allocator y) noexcept
    {
      return !(x == y);
    }

    // It might get instantiated but should not be called.
    //
    small_allocator
    select_on_container_copy_construction () const noexcept
    {
      assert (false);
      return small_allocator (nullptr);
    }

    // propagate_on_container_copy_assignment = false
    // propagate_on_container_move_assignment = false

    // Swap is not supported (see explanation in small_vector::swap()).
    //
    using propagate_on_container_swap = std::true_type;

    void
    swap (small_allocator&) = delete;

  private:
    template <typename, std::size_t, typename>
    friend class small_allocator; // For buffer access in rebind.

    buffer_type* buf_; // Must not be NULL.
  };
}
