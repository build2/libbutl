/**
* std::shared_mutex et al implementation for MinGW-w64
*
* Copyright (c) 2017 by Nathaniel J. McClatchey, Athens OH, United States
* Copyright (c) 2022 the build2 authors
*
* Licensed under the simplified (2-clause) BSD License.
* You should have received a copy of the license along with this
* program.
*
* This code is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#ifndef LIBBUTL_MINGW_SHARED_MUTEX_HXX
#define LIBBUTL_MINGW_SHARED_MUTEX_HXX

#if !defined(__cplusplus) || (__cplusplus < 201402L)
#  error C++14 compiler required
#endif

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601
#  error _WIN32_WINNT should be 0x0601 (Windows 7) or greater
#endif

#include <cassert>
//  For descriptive errors.
#include <system_error>
//  For timing in shared_timed_mutex.
#include <chrono>
#include <limits>

#include <shared_mutex> // shared_lock

//  For defer_lock_t, adopt_lock_t, and try_to_lock_t
#include <libbutl/mingw-mutex.hxx>

#include <synchapi.h>

namespace mingw_stdthread
{
  using std::shared_lock;

  class condition_variable_any;

  // Slim Reader-Writer (SRW)-based implementation that requires Windows 7.
  //
  class shared_mutex : mutex
  {
    friend class condition_variable_any;
  public:
    using mutex::native_handle_type;
    using mutex::lock;
    using mutex::try_lock;
    using mutex::unlock;
    using mutex::native_handle;

    void lock_shared ()
    {
      AcquireSRWLockShared(&mHandle);
    }

    void unlock_shared ()
    {
      ReleaseSRWLockShared(&mHandle);
    }

    bool try_lock_shared ()
    {
      return TryAcquireSRWLockShared(&mHandle) != 0;
    }
  };

  class shared_timed_mutex : shared_mutex
  {
    typedef shared_mutex Base;
  public:
    using Base::lock;
    using Base::try_lock;
    using Base::unlock;
    using Base::lock_shared;
    using Base::try_lock_shared;
    using Base::unlock_shared;

    template< class Clock, class Duration >
    bool try_lock_until ( const std::chrono::time_point<Clock,Duration>& cutoff )
    {
      do
      {
        if (try_lock())
          return true;
      }
      while (std::chrono::steady_clock::now() < cutoff);
      return false;
    }

    template< class Rep, class Period >
    bool try_lock_for (const std::chrono::duration<Rep,Period>& rel_time)
    {
      return try_lock_until(std::chrono::steady_clock::now() + rel_time);
    }

    template< class Clock, class Duration >
    bool try_lock_shared_until ( const std::chrono::time_point<Clock,Duration>& cutoff )
    {
      do
      {
        if (try_lock_shared())
          return true;
      }
      while (std::chrono::steady_clock::now() < cutoff);
      return false;
    }

    template< class Rep, class Period >
    bool try_lock_shared_for (const std::chrono::duration<Rep,Period>& rel_time)
    {
      return try_lock_shared_until(std::chrono::steady_clock::now() + rel_time);
    }
  };
}

#endif // LIBBUTL_MINGW_SHARED_MUTEX_HXX
