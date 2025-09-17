// file      : libbutl/builtin.txx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

namespace butl
{
#ifdef LIBBUTL_BUILTIN_POSIX_THREADS
  template <typename F>
  builtin::async_state::
  async_state (std::uint8_t& r, F f, optional<std::size_t> max_stack)
  {
    pthread_attr_t attr;
    if (int r = pthread_attr_init (&attr))
      throw_system_error (r);

    std::unique_ptr<pthread_attr_t, pthread_attr_deleter> ad (&attr);
    if (int r = pthread_attr_setstacksize (&attr, stack_size (max_stack)))
      throw_system_error (r);

    using thunk = async_thunk<F>;
    std::unique_ptr<thunk> t (new thunk {this, std::move (f), &r});

    if (int r = pthread_create (&thread_, &attr, thunk::execute, t.get ()))
      throw_system_error (r);

    t.release (); // Is now owned by thunk::execute().

    thread_joinable_ = true;
  }

  template <typename F>
  void* builtin::async_state::async_thunk<F>::
  execute (void* p) noexcept
  {
    std::unique_ptr<async_thunk> t (static_cast<async_thunk*> (p));
    t->s->execute (*t->r, t->f);
    return nullptr;
  }
#endif
}
