/**
* std::condition_variable implementation for MinGW-w64
*
* Copyright (c) 2013-2016 by Mega Limited, Auckland, New Zealand
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

#ifndef LIBBUTL_MINGW_CONDITION_VARIABLE_HXX
#define LIBBUTL_MINGW_CONDITION_VARIABLE_HXX

#if !defined(__cplusplus) || (__cplusplus < 201402L)
#  error C++14 compiler required
#endif

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601
#  error _WIN32_WINNT should be 0x0601 (Windows 7) or greater
#endif

#include <condition_variable> // Use std::cv_status, if available.

#include <cassert>
#include <chrono>
#include <system_error>

#include <synchapi.h>

#include <libbutl/mingw-mutex.hxx>
#include <libbutl/mingw-shared_mutex.hxx>

namespace mingw_stdthread
{
#if defined(__MINGW32__ ) && !defined(_GLIBCXX_HAS_GTHREADS)
  enum class cv_status { no_timeout, timeout };
#else
  using std::cv_status;
#endif

  //  Native condition variable-based implementation.
  //
  class condition_variable
  {
    static constexpr DWORD kInfinite = 0xffffffffl;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
    CONDITION_VARIABLE cvariable_ = CONDITION_VARIABLE_INIT;
#pragma GCC diagnostic pop

    friend class condition_variable_any;

    bool wait_unique (mutex * pmutex, DWORD time)
    {
      BOOL success = SleepConditionVariableSRW(native_handle(),
                                               pmutex->native_handle(),
                                               time,
//    CONDITION_VARIABLE_LOCKMODE_SHARED has a value not specified by
//  Microsoft's Dev Center, but is known to be (convertible to) a ULONG. To
//  ensure that the value passed to this function is not equal to Microsoft's
//  constant, we can either use a static_assert, or simply generate an
//  appropriate value.
                                               !CONDITION_VARIABLE_LOCKMODE_SHARED);
      return success;
    }
    bool wait_impl (unique_lock<mutex> & lock, DWORD time)
    {
      mutex * pmutex = lock.release();
      bool success = wait_unique(pmutex, time);
      lock = unique_lock<mutex>(*pmutex, adopt_lock);
      return success;
    }
public:
    using native_handle_type = PCONDITION_VARIABLE;
    native_handle_type native_handle ()
    {
      return &cvariable_;
    }

    condition_variable () = default;
    ~condition_variable () = default;

    condition_variable (const condition_variable &) = delete;
    condition_variable & operator= (const condition_variable &) = delete;

    void notify_one () noexcept
    {
      WakeConditionVariable(&cvariable_);
    }

    void notify_all () noexcept
    {
      WakeAllConditionVariable(&cvariable_);
    }

    void wait (unique_lock<mutex> & lock)
    {
      wait_impl(lock, kInfinite);
    }

    template<class Predicate>
    void wait (unique_lock<mutex> & lock, Predicate pred)
    {
      while (!pred())
        wait(lock);
    }

    template <class Rep, class Period>
    cv_status wait_for(unique_lock<mutex>& lock,
                       const std::chrono::duration<Rep, Period>& rel_time)
    {
      using namespace std::chrono;
      auto timeout = duration_cast<milliseconds>(rel_time).count();
      DWORD waittime = (timeout < kInfinite) ? ((timeout < 0) ? 0 : static_cast<DWORD>(timeout)) : (kInfinite - 1);
      bool result = wait_impl(lock, waittime) || (timeout >= kInfinite);
      return result ? cv_status::no_timeout : cv_status::timeout;
    }

    template <class Rep, class Period, class Predicate>
    bool wait_for(unique_lock<mutex>& lock,
                  const std::chrono::duration<Rep, Period>& rel_time,
                  Predicate pred)
    {
#if __cplusplus >= 201703L
      using steady_duration = typename std::chrono::steady_clock::duration;
      return wait_until(lock,
                        std::chrono::steady_clock::now() +
                        std::chrono::ceil<steady_duration> (rel_time),
                        std::move(pred));
#else
      return wait_until(lock,
                        std::chrono::steady_clock::now() + rel_time,
                        std::move(pred));
#endif
    }
    template <class Clock, class Duration>
    cv_status wait_until (unique_lock<mutex>& lock,
                          const std::chrono::time_point<Clock,Duration>& abs_time)
    {
      return wait_for(lock, abs_time - Clock::now());
    }
    template <class Clock, class Duration, class Predicate>
    bool wait_until  (unique_lock<mutex>& lock,
                      const std::chrono::time_point<Clock, Duration>& abs_time,
                      Predicate pred)
    {
        while (!pred())
        {
          if (wait_until(lock, abs_time) == cv_status::timeout)
          {
            return pred();
          }
        }
        return true;
    }
  };

  class condition_variable_any
  {
    static constexpr DWORD kInfinite = 0xffffffffl;

    condition_variable internal_cv_ {};
    mutex internal_mutex_ {};

    template<class L>
    bool wait_impl (L & lock, DWORD time)
    {
      unique_lock<decltype(internal_mutex_)> internal_lock(internal_mutex_);
      lock.unlock();
      bool success = internal_cv_.wait_impl(internal_lock, time);
      lock.lock();
      return success;
    }
    // If the lock happens to be called on a native Windows mutex, skip any
    // extra contention.
    inline bool wait_impl (unique_lock<mutex> & lock, DWORD time)
    {
        return internal_cv_.wait_impl(lock, time);
    }
    bool wait_impl (unique_lock<shared_mutex> & lock, DWORD time)
    {
      shared_mutex * pmutex = lock.release();
      bool success = internal_cv_.wait_unique(pmutex, time);
      lock = unique_lock<shared_mutex>(*pmutex, adopt_lock);
      return success;
    }
    bool wait_impl (shared_lock<shared_mutex> & lock, DWORD time)
    {
      shared_mutex * pmutex = lock.release();
      BOOL success = SleepConditionVariableSRW(native_handle(),
                                               pmutex->native_handle(), time,
                                               CONDITION_VARIABLE_LOCKMODE_SHARED);
      lock = shared_lock<shared_mutex>(*pmutex, adopt_lock);
      return success;
    }
  public:
    using native_handle_type = typename condition_variable::native_handle_type;

    native_handle_type native_handle ()
    {
      return internal_cv_.native_handle();
    }

    void notify_one () noexcept
    {
      internal_cv_.notify_one();
    }

    void notify_all () noexcept
    {
      internal_cv_.notify_all();
    }

    condition_variable_any () = default;
    ~condition_variable_any () = default;

    template<class L>
    void wait (L & lock)
    {
      wait_impl(lock, kInfinite);
    }

    template<class L, class Predicate>
    void wait (L & lock, Predicate pred)
    {
      while (!pred())
        wait(lock);
    }

    template <class L, class Rep, class Period>
    cv_status wait_for(L& lock, const std::chrono::duration<Rep,Period>& period)
    {
      using namespace std::chrono;
      auto timeout = duration_cast<milliseconds>(period).count();
      DWORD waittime = (timeout < kInfinite) ? ((timeout < 0) ? 0 : static_cast<DWORD>(timeout)) : (kInfinite - 1);
      bool result = wait_impl(lock, waittime) || (timeout >= kInfinite);
      return result ? cv_status::no_timeout : cv_status::timeout;
    }

    template <class L, class Rep, class Period, class Predicate>
    bool wait_for(L& lock, const std::chrono::duration<Rep, Period>& period,
                  Predicate pred)
    {
      return wait_until(lock, std::chrono::steady_clock::now() + period,
                        std::move(pred));
    }
    template <class L, class Clock, class Duration>
    cv_status wait_until (L& lock,
                          const std::chrono::time_point<Clock,Duration>& abs_time)
    {
      return wait_for(lock, abs_time - Clock::now());
    }
    template <class L, class Clock, class Duration, class Predicate>
    bool wait_until  (L& lock,
                      const std::chrono::time_point<Clock, Duration>& abs_time,
                      Predicate pred)
    {
      while (!pred())
      {
        if (wait_until(lock, abs_time) == cv_status::timeout)
        {
          return pred();
        }
      }
      return true;
    }
  };
}

#endif // LIBBUTL_MINGW_CONDITION_VARIABLE_HXX
