// file      : libbutl/move-only-function.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <utility>
#include <functional>
#include <type_traits>

namespace butl
{
  // This is a move-only std::function version which is implemented in terms
  // of std::function. It is similar to C++23 std::move_only_function but
  // still provides target() (but not target_type()).
  //
  template <typename>
  class move_only_function_ex;

  // Alias butl::move_only_function to std::move_only_function if available
  // and to move_only_function_ex otherwise.
  //
#ifdef __cpp_lib_move_only_function
  using std::move_only_function;
#else
  template <typename F>
  using move_only_function = move_only_function_ex<F>;
#endif

  template <typename R, typename... A>
  class move_only_function_ex<R (A...)>
  {
  public:
    using result_type = R;

    move_only_function_ex () = default;
    move_only_function_ex (std::nullptr_t) noexcept {}

    // Note: according to the spec we should also disable these if F is not
    // callable, but that is not easy to do in C++14. Maybe we should do
    // something for C++17 and later (without this the diagnostics is quite
    // hairy).
    //
    template <typename F>
    move_only_function_ex (F&& f, typename std::enable_if<!std::is_same<typename std::remove_reference<F>::type, move_only_function_ex>::value>::type* = 0)
    {
      using FV = typename std::decay<F>::type;

      if (!null (f))
        f_ = wrapper<FV> (std::forward<F> (f));
    }

    template <typename F>
    typename std::enable_if<!std::is_same<typename std::remove_reference<F>::type, move_only_function_ex>::value, move_only_function_ex>::type&
    operator= (F&& f)
    {
      move_only_function_ex (std::forward<F> (f)).swap (*this);
      return *this;
    }

    move_only_function_ex&
    operator= (std::nullptr_t) noexcept
    {
      f_ = nullptr;
      return *this;
    }

    void swap (move_only_function_ex& f) noexcept
    {
      f_.swap (f.f_);
    }

    R operator() (A... args) const
    {
      return f_ (std::forward<A> (args)...);
    }

    explicit operator bool () const noexcept
    {
      return static_cast<bool> (f_);
    }

    template <typename T>
    T* target() noexcept
    {
      wrapper<T>* r (f_.template target<wrapper<T>> ());
      return r != nullptr ? &r->f : nullptr;
    }

    template <typename T>
    const T* target() const noexcept
    {
      const wrapper<T>* r (f_.template target<wrapper<T>> ());
      return r != nullptr ? &r->f : nullptr;
    }

    move_only_function_ex (move_only_function_ex&&) = default;
    move_only_function_ex& operator= (move_only_function_ex&&) = default;

    move_only_function_ex (const move_only_function_ex&) = delete;
    move_only_function_ex& operator= (const move_only_function_ex&) = delete;

  private:
    template <typename F>
    struct wrapper
    {
      struct empty {};

      union
      {
        F f;
        empty e;
      };

      explicit wrapper (F&& f_): f (std::move (f_)) {}
      explicit wrapper (const F& f_): f (f_) {}

      R operator() (A... args)
      {
        return f (std::forward<A> (args)...);
      }

      R operator() (A... args) const
      {
        return f (std::forward<A> (args)...);
      }

      wrapper (wrapper&& w)
        noexcept (std::is_nothrow_move_constructible<F>::value)
        : f (std::move (w.f)) {}

      wrapper& operator= (wrapper&&) = delete; // Shouldn't be needed.

      ~wrapper () {f.~F ();}

      // These shouldn't be called.
      //
      wrapper (const wrapper&) {}
      wrapper& operator= (const wrapper&) {return *this;}
    };

    template <typename F>                              static bool null (const F&) {return false;}
    template <typename R1, typename... A1>             static bool null (R1 (*p) (A1...)) {return p == nullptr;}
    template <typename R1, typename... A1>             static bool null (const move_only_function_ex<R1 (A1...)>& f) {return !f;}
    template <typename R1, typename C, typename... A1> static bool null (R1 (C::*p) (A1...)) {return p == nullptr;}
    template <typename R1, typename C, typename... A1> static bool null (R1 (C::*p) (A1...) const) {return p == nullptr;}

    std::function<R (A...)> f_;
  };

  template <typename R, typename... A>
  inline bool
  operator== (const move_only_function_ex<R (A...)>& f, std::nullptr_t) noexcept
  {
    return !f;
  }

  template <typename R, typename... A>
  inline bool
  operator== (std::nullptr_t, const move_only_function_ex<R (A...)>& f) noexcept
  {
    return !f;
  }

  template <typename R, typename... A>
  inline bool
  operator!= (const move_only_function_ex<R (A...)>& f, std::nullptr_t) noexcept
  {
    return static_cast<bool> (f);
  }

  template <typename R, typename... A>
  inline bool
  operator!= (std::nullptr_t, const move_only_function_ex<R (A...)>& f) noexcept
  {
    return static_cast<bool> (f);
  }
}
