// file      : libbutl/builtin.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

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
  template <typename F>
  inline builtin::async_state::
  async_state (uint8_t& r, F f)
      : thread ([this, &r, f = std::move (f)] () mutable noexcept
                {
                  uint8_t t (f ());

                  {
                    unique_lock l (this->mutex);
                    r = t;
                    finished = true;
                  }

                  condv.notify_all ();
                })
  {
  }

  template <typename F>
  inline builtin
  pseudo_builtin (std::uint8_t& r, F f)
  {
    std::unique_ptr<builtin::async_state> s (
      new builtin::async_state (
        r,
        [f = std::move (f)] () mutable noexcept -> uint8_t
        {
          return f ();
        }));

    return builtin (r, move (s));
  }
}
