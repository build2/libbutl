// file      : libbutl/process-details.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <libbutl/ft/shared_mutex.hxx>

#ifdef __cpp_lib_modules
import std.core; //@@ MOD TMP (dummy std.threading).
import std.threading;
#else
#include <mutex>
#if defined(__cpp_lib_shared_mutex) || defined(__cpp_lib_shared_timed_mutex)
#  include <shared_mutex>
#endif
#endif

namespace butl
{
#if   defined(__cpp_lib_shared_mutex)
  using shared_mutex = std::shared_mutex;
  using ulock        = std::unique_lock<shared_mutex>;
  using slock        = std::shared_lock<shared_mutex>;
#elif defined(__cpp_lib_shared_timed_mutex)
  using shared_mutex = std::shared_timed_mutex;
  using ulock        = std::unique_lock<shared_mutex>;
  using slock        = std::shared_lock<shared_mutex>;
#else
  // Because we have this fallback, we need to be careful not to create
  // multiple shared locks in the same thread.
  //
  struct shared_mutex: std::mutex
  {
    using mutex::mutex;

    void lock_shared     () { lock ();     }
    void try_lock_shared () { try_lock (); }
    void unlock_shared   () { unlock ();   }
  };

  using ulock        = std::unique_lock<shared_mutex>;
  using slock        = ulock;
#endif

  // Mutex that is acquired to make a sequence of operations atomic in regards
  // to child process spawning. Must be aquired for exclusive access for child
  // process startup, and for shared access otherwise. Defined in process.cxx.
  //
  extern shared_mutex process_spawn_mutex;
}
