// file      : libbutl/builtin.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifdef LIBBUTL_BUILTIN_POSIX_THREADS
#  include <system_error>
#endif

namespace butl
{
  // builtin
  //
  // Implement timed_wait() function templates in terms of their milliseconds
  // specialization.
  //
  template <>
  LIBBUTL_SYMEXPORT optional<std::uint8_t> builtin::
  timed_wait (const std::chrono::milliseconds&);

  template <typename R, typename P>
  inline optional<std::uint8_t> builtin::
  timed_wait (const std::chrono::duration<R, P>& d)
  {
    using namespace std::chrono;
    return timed_wait (duration_cast<milliseconds> (d));
  }

  inline optional<std::uint8_t> builtin::
  try_wait ()
  {
    if (state_ != nullptr)
    {
      unique_lock l (state_->mutex);

      if (!state_->finished)
        return nullopt;
    }

    return result_;
  }

  // builtin_map
  //
  inline const builtin_info* builtin_map::
  find (const std::string& n) const
  {
    auto i (base::find (n));
    return i != end () ? &i->second : nullptr;
  }

  // builtin::async_state
  //
#ifndef LIBBUTL_BUILTIN_POSIX_THREADS
  template <typename F>
  inline builtin::async_state::
  async_state (std::uint8_t& r, F f, optional<std::size_t> /* max_stack */)
      : thread_ ([this, &r, f = std::move (f)] () mutable noexcept
                 {
                   execute (r, f);
                 })
  {
  }
#endif

  template <typename F>
  inline void builtin::async_state::
  execute (std::uint8_t& r, F& f) noexcept
  {
    std::uint8_t t (f ());

    {
      unique_lock l (this->mutex);
      r = t;
      finished = true;
    }

    condv.notify_all ();
  }

  inline void builtin::async_state::
  join (bool ignore_error)
  {
#ifndef LIBBUTL_BUILTIN_POSIX_THREADS
    if (thread_.joinable ())
    try
    {
      thread_.join ();
    }
    catch (const std::system_error&)
    {
      if (!ignore_error)
        throw;
    }
#else
    if (thread_joinable_)
    {
      // There is no reason to think that the thread can be joined after the
      // failed attempt. Thus, let's always reset this flag.
      //
      thread_joinable_ = false;

      if (int r = pthread_join (thread_, nullptr /* retval */))
      {
        if (!ignore_error)
          throw_system_error (r);
      }
    }
#endif
  }

  // pseudo_builtin()
  //
  template <typename F>
  inline builtin
  pseudo_builtin (std::uint8_t& r, F f, optional<std::size_t> max_stack)
  {
    std::unique_ptr<builtin::async_state> s (
      new builtin::async_state (
        r,
        [f = std::move (f)] () mutable noexcept -> std::uint8_t
        {
          return f ();
        },
        max_stack));

    return builtin (r, move (s));
  }
}
