// file      : libbutl/mutex.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#ifndef LIBBUTL_MINGW_STDTHREAD
#  include <mutex>

#  include <libbutl/ft/shared_mutex.hxx>
#  if defined(__cpp_lib_shared_mutex) || defined(__cpp_lib_shared_timed_mutex)
#    include <shared_mutex>
#  endif

namespace butl
{
  using std::mutex;
  using mlock = std::unique_lock<mutex>;

  using std::defer_lock;
  using std::adopt_lock;

#  if   defined(__cpp_lib_shared_mutex)
  using shared_mutex = std::shared_mutex;
  using ulock        = std::unique_lock<shared_mutex>;
  using slock        = std::shared_lock<shared_mutex>;
#  elif defined(__cpp_lib_shared_timed_mutex)
  using shared_mutex = std::shared_timed_mutex;
  using ulock        = std::unique_lock<shared_mutex>;
  using slock        = std::shared_lock<shared_mutex>;
#  else
  // Because we have this fallback, we need to be careful not to create
  // multiple shared locks in the same thread.
  //
  struct shared_mutex: mutex
  {
    using mutex::mutex;

    void lock_shared     () { lock ();     }
    void try_lock_shared () { try_lock (); }
    void unlock_shared   () { unlock ();   }
  };

  using ulock        = std::unique_lock<shared_mutex>;
  using slock        = ulock;
#  endif
}
#else  // LIBBUTL_MINGW_STDTHREAD
#  include <libbutl/mingw-mutex.hxx>
#  include <libbutl/mingw-shared_mutex.hxx>

namespace butl
{
  using mingw_stdthread::mutex;
  using mlock = mingw_stdthread::unique_lock<mutex>;

  using mingw_stdthread::defer_lock;
  using mingw_stdthread::adopt_lock;

  using shared_mutex = mingw_stdthread::shared_mutex;
  using ulock        = mingw_stdthread::unique_lock<shared_mutex>;
  using slock        = mingw_stdthread::shared_lock<shared_mutex>;
}
#endif // LIBBUTL_MINGW_STDTHREADS
