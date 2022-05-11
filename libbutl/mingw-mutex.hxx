/**
* std::mutex et al implementation for MinGW-w64
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

#ifndef LIBBUTL_MINGW_MUTEX_HXX
#define LIBBUTL_MINGW_MUTEX_HXX

#if !defined(__cplusplus) || (__cplusplus < 201402L)
#  error C++14 compiler required
#endif

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601
#  error _WIN32_WINNT should be 0x0601 (Windows 7) or greater
#endif

#include <chrono>
#include <system_error>
#include <atomic>

#include <mutex>

#include <synchapi.h>       //  For InitializeCriticalSection, etc.
#include <errhandlingapi.h> //  For GetLastError
#include <handleapi.h>

namespace mingw_stdthread
{
  // To make this namespace equivalent to the thread-related subset of std,
  // pull in the classes and class templates supplied by std but not by this
  // implementation.
  //
  using std::lock_guard;
  using std::unique_lock;
  using std::adopt_lock_t;
  using std::defer_lock_t;
  using std::try_to_lock_t;
  using std::adopt_lock;
  using std::defer_lock;
  using std::try_to_lock;

  class recursive_mutex
  {
    CRITICAL_SECTION mHandle;
  public:
    typedef LPCRITICAL_SECTION native_handle_type;
    native_handle_type native_handle() {return &mHandle;}
    recursive_mutex() noexcept : mHandle()
    {
      InitializeCriticalSection(&mHandle);
    }
    recursive_mutex (const recursive_mutex&) = delete;
    recursive_mutex& operator=(const recursive_mutex&) = delete;
    ~recursive_mutex() noexcept
    {
      DeleteCriticalSection(&mHandle);
    }
    void lock()
    {
      EnterCriticalSection(&mHandle);
    }
    void unlock()
    {
      LeaveCriticalSection(&mHandle);
    }
    bool try_lock()
    {
      return (TryEnterCriticalSection(&mHandle)!=0);
    }
  };

  // Slim Reader-Writer (SRW)-based implementation that requires Windows 7.
  //
  class mutex
  {
  protected:
    SRWLOCK mHandle;
  public:
    typedef PSRWLOCK native_handle_type;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
    constexpr mutex () noexcept : mHandle(SRWLOCK_INIT) { }
#pragma GCC diagnostic pop
    mutex (const mutex&) = delete;
    mutex & operator= (const mutex&) = delete;
    void lock ()
    {
      AcquireSRWLockExclusive(&mHandle);
    }
    void unlock ()
    {
      ReleaseSRWLockExclusive(&mHandle);
    }
    //  TryAcquireSRW functions are a Windows 7 feature.
    bool try_lock ()
    {
      BOOL ret = TryAcquireSRWLockExclusive(&mHandle);
      return ret;
    }
    native_handle_type native_handle ()
    {
        return &mHandle;
    }
  };

  class recursive_timed_mutex
  {
    static constexpr DWORD kWaitAbandoned = 0x00000080l;
    static constexpr DWORD kWaitObject0 = 0x00000000l;
    static constexpr DWORD kInfinite = 0xffffffffl;
    inline bool try_lock_internal (DWORD ms) noexcept
    {
      DWORD ret = WaitForSingleObject(mHandle, ms);

      /*
        @@ TODO
#ifndef NDEBUG
      if (ret == kWaitAbandoned)
      {
        using namespace std;
        fprintf(stderr, "FATAL: Thread terminated while holding a mutex.");
        terminate();
      }
#endif
      */

      return (ret == kWaitObject0) || (ret == kWaitAbandoned);
    }
  protected:
    HANDLE mHandle;
  public:
    typedef HANDLE native_handle_type;
    native_handle_type native_handle() const {return mHandle;}
    recursive_timed_mutex(const recursive_timed_mutex&) = delete;
    recursive_timed_mutex& operator=(const recursive_timed_mutex&) = delete;
    recursive_timed_mutex(): mHandle(CreateMutex(NULL, FALSE, NULL)) {}
    ~recursive_timed_mutex()
    {
      CloseHandle(mHandle);
    }
    void lock()
    {
      DWORD ret = WaitForSingleObject(mHandle, kInfinite);

      /*
        @@ TODO

//    If (ret == WAIT_ABANDONED), then the thread that held ownership was
//  terminated. Behavior is undefined, but Windows will pass ownership to this
//  thread.
#ifndef NDEBUG
        if (ret == kWaitAbandoned)
        {
            using namespace std;
            fprintf(stderr, "FATAL: Thread terminated while holding a mutex.");
            terminate();
        }
#endif
      */

      if ((ret != kWaitObject0) && (ret != kWaitAbandoned))
      {
        throw std::system_error(GetLastError(), std::system_category());
      }
    }
    void unlock()
    {
      if (!ReleaseMutex(mHandle))
        throw std::system_error(GetLastError(), std::system_category());
    }
    bool try_lock()
    {
      return try_lock_internal(0);
    }
    template <class Rep, class Period>
    bool try_lock_for(const std::chrono::duration<Rep,Period>& dur)
    {
      using namespace std::chrono;
      auto timeout = duration_cast<milliseconds>(dur).count();
      while (timeout > 0)
      {
        constexpr auto kMaxStep = static_cast<decltype(timeout)>(kInfinite-1);
        auto step = (timeout < kMaxStep) ? timeout : kMaxStep;
        if (try_lock_internal(static_cast<DWORD>(step)))
          return true;
        timeout -= step;
      }
      return false;
    }
    template <class Clock, class Duration>
    bool try_lock_until(const std::chrono::time_point<Clock,Duration>& timeout_time)
    {
      return try_lock_for(timeout_time - Clock::now());
    }
  };

  typedef recursive_timed_mutex timed_mutex;
}

#endif // LIBBUTL_MINGW_MUTEX_HXX
