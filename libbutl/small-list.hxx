// file      : libbutl/small-list.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <list>
#include <cstddef>     // size_t
#include <utility>     // move()
#include <type_traits> // is_nothrow_move_constructible

#include <libbutl/small-allocator.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // Issues and limitations.
  //
  // - VC's implementation of std::list allocates an extra "headnode"
  //   (something to do with additional iterator stability guarantees). Which
  //   means only empty small list will actually be "small". As a result,
  //   unless you don't care about VC, you should use small_forward_list
  //   instead.
  //
  // - Because small_allocator currently expects us to allocate the entire
  //   small buffer and there is no reserve() in std::list, we currently
  //   only support N==1 (which we static_assert).
  //
  // - swap() is deleted (see notes below).
  //
  // - The implementation doesn't allocate T but rather a "node" that normally
  //   consists of two pointers (prev/next) and T.
  //
  template <typename T>
  using small_list_node =
#if   defined (_MSC_VER)
  std::_List_node<T, void*>;
#elif defined (__GLIBCXX__)
  std::_List_node<T>;
#elif defined (_LIBCPP_VERSION)
  std::__list_node<T, void*>;
#else
#error unknown standard library implementation
#endif

  template <typename T, std::size_t N>
  using small_list_buffer = small_allocator_buffer<small_list_node<T>, N>;

  template <typename T, std::size_t N>
  class small_list:
    private small_list_buffer<T, N>,
    public std::list<T, small_allocator<T, N, small_list_buffer<T, N>>>
  {
    static_assert (N == 1, "only small_list or 1 currently supported");

  public:
    static constexpr const std::size_t small_size = N;

    using buffer_type = small_list_buffer<T, N>;
    using allocator_type = small_allocator<T, N, buffer_type>;
    using base_type = std::list<T, allocator_type>;

    small_list ()
      : base_type (allocator_type (this)) {}

    small_list (std::initializer_list<T> v)
      : base_type (allocator_type (this))
    {
      static_cast<base_type&> (*this) = v;
    }

    template <typename I>
    small_list (I b, I e)
      : base_type (allocator_type (this))
    {
      this->assign (b, e);
    }

    explicit
    small_list (std::size_t n)
      : base_type (allocator_type (this))
    {
      this->resize (n);
    }

    small_list (std::size_t n, const T& x)
      : base_type (allocator_type (this))
    {
      this->assign (n, x);
    }

    small_list (const small_list& v)
      : buffer_type (), base_type (allocator_type (this))
    {
      static_cast<base_type&> (*this) = v;
    }

    small_list&
    operator= (const small_list& v)
    {
      // Note: propagate_on_container_copy_assignment = false
      //
      static_cast<base_type&> (*this) = v;
      return *this;
    }

    // See small_vector for the move-constructor/assignment noexept
    // expressions reasoning.
    //
    small_list (small_list&& v)
#if !defined(__GLIBCXX__) && (!defined(_MSC_VER) || _MSC_VER > 1900)
      noexcept (std::is_nothrow_move_constructible<T>::value)
#endif
      : base_type (allocator_type (this))
    {
      *this = std::move (v); // Delegate to operator=(&&).
    }

    small_list&
    operator= (small_list&& v) noexcept (false)
    {
      // libstdc++'s implementation prior to GCC 6 is broken (calls swap()).
      // Since there is no easy way to determine this library's version, for
      // now this is always enabled.
      //
      // Similarly, VC14's implementation of operator=(&&) swaps pointers
      // without regard for allocator (fixed in 15).
      //
#if defined(__GLIBCXX__) || (defined(_MSC_VER) && _MSC_VER <= 1900)
      this->clear ();
      for (T& x: v)
        this->push_back (std::move (x)); // Note: can throw bad_alloc.
      v.clear ();
#else
      // Note: propagate_on_container_move_assignment = false
      //
      static_cast<base_type&> (*this) = std::move (v);
#endif

      return *this;
    }

    small_list&
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
    swap (small_list&) = delete;
  };
}
