/**
* std::thread implementation for MinGW-w64
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

#ifndef LIBBUTL_MINGW_THREAD_HXX
#define LIBBUTL_MINGW_THREAD_HXX

#if !defined(__cplusplus) || (__cplusplus < 201402L)
#  error C++14 compiler required
#endif

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601
#  error _WIN32_WINNT should be 0x0601 (Windows 7) or greater
#endif

#include <cstddef>      //  For std::size_t
#include <cerrno>       //  Detect error type.
#include <exception>    //  For std::terminate
#include <system_error> //  For std::system_error
#include <functional>   //  For std::hash, std::invoke (C++17)
#include <tuple>        //  For std::tuple
#include <chrono>       //  For sleep timing.
#include <memory>       //  For std::unique_ptr
#include <iosfwd>       //  Stream output for thread ids.
#include <utility>      //  For std::swap, std::forward

#include <synchapi.h>   //  For WaitForSingleObject
#include <handleapi.h>  //  For CloseHandle, etc.
#include <sysinfoapi.h> //  For GetNativeSystemInfo
#include <processthreadsapi.h>  //  For GetCurrentThreadId

#include <process.h>  //  For _beginthreadex

#if __cplusplus < 201703L
#  include <libbutl/mingw-invoke.hxx>
#endif

namespace mingw_stdthread
{
  // @@ I think can get rid of this in C++14.
  //
  namespace detail
  {
    template<std::size_t...>
    struct IntSeq {};

    template<std::size_t N, std::size_t... S>
    struct GenIntSeq : GenIntSeq<N-1, N-1, S...> { };

    template<std::size_t... S>
    struct GenIntSeq<0, S...> { typedef IntSeq<S...> type; };

//    Use a template specialization to avoid relying on compiler optimization
//  when determining the parameter integer sequence.
    template<class Func, class T, typename... Args>
    class ThreadFuncCall;
// We can't define the Call struct in the function - the standard forbids template methods in that case
    template<class Func, std::size_t... S, typename... Args>
    class ThreadFuncCall<Func, detail::IntSeq<S...>, Args...>
    {
        static_assert(sizeof...(S) == sizeof...(Args), "Args must match.");
        using Tuple = std::tuple<typename std::decay<Args>::type...>;
        typename std::decay<Func>::type mFunc;
        Tuple mArgs;

    public:
        ThreadFuncCall(Func&& aFunc, Args&&... aArgs)
          : mFunc(std::forward<Func>(aFunc)),
            mArgs(std::forward<Args>(aArgs)...)
        {
        }

        void callFunc()
        {
#if __cplusplus < 201703L
          detail::invoke(std::move(mFunc), std::move(std::get<S>(mArgs)) ...);
#else
          std::invoke (std::move(mFunc), std::move(std::get<S>(mArgs)) ...);
#endif
        }
    };

    // Allow construction of threads without exposing implementation.
    class ThreadIdTool;
  }

  class thread
  {
  public:
    class id
    {
      DWORD mId = 0;
      friend class thread;
      friend class std::hash<id>;
      friend class detail::ThreadIdTool;
      explicit id(DWORD aId) noexcept : mId(aId){}
    public:
      id () noexcept = default;
      friend bool operator==(id x, id y) noexcept {return x.mId == y.mId; }
      friend bool operator!=(id x, id y) noexcept {return x.mId != y.mId; }
      friend bool operator< (id x, id y) noexcept {return x.mId <  y.mId; }
      friend bool operator<=(id x, id y) noexcept {return x.mId <= y.mId; }
      friend bool operator> (id x, id y) noexcept {return x.mId >  y.mId; }
      friend bool operator>=(id x, id y) noexcept {return x.mId >= y.mId; }

      template<class _CharT, class _Traits>
      friend std::basic_ostream<_CharT, _Traits>&
      operator<<(std::basic_ostream<_CharT, _Traits>& __out, id __id)
      {
        if (__id.mId == 0)
        {
          return __out << "<invalid std::thread::id>";
        }
        else
        {
          return __out << __id.mId;
        }
      }
    };
  private:
    static constexpr HANDLE kInvalidHandle = nullptr;
    static constexpr DWORD kInfinite = 0xffffffffl;
    HANDLE mHandle;
    id mThreadId;

    template <class Call>
    static unsigned __stdcall threadfunc(void* arg)
    {
      std::unique_ptr<Call> call(static_cast<Call*>(arg));
      call->callFunc();
      return 0;
    }

    static unsigned int _hardware_concurrency_helper() noexcept
    {
      SYSTEM_INFO sysinfo;
      ::GetNativeSystemInfo(&sysinfo);
      return sysinfo.dwNumberOfProcessors;
    }
  public:
    typedef HANDLE native_handle_type;
    id get_id() const noexcept {return mThreadId;}
    native_handle_type native_handle() const {return mHandle;}
    thread(): mHandle(kInvalidHandle), mThreadId(){}

    thread(thread&& other) noexcept
        :mHandle(other.mHandle), mThreadId(other.mThreadId)
    {
      other.mHandle = kInvalidHandle;
      other.mThreadId = id{};
    }

    thread(const thread &other) = delete;

    template<class Func, typename... Args>
    explicit thread(Func&& func, Args&&... args) : mHandle(), mThreadId()
    {
      // Instead of INVALID_HANDLE_VALUE, _beginthreadex returns 0.

      using ArgSequence = typename detail::GenIntSeq<sizeof...(Args)>::type;
      using Call = detail::ThreadFuncCall<Func, ArgSequence, Args...>;
      auto call = new Call(std::forward<Func>(func), std::forward<Args>(args)...);
      unsigned int id_receiver;
      auto int_handle = _beginthreadex(NULL, 0, threadfunc<Call>,
                                       static_cast<LPVOID>(call), 0, &id_receiver);
      if (int_handle == 0)
      {
        mHandle = kInvalidHandle;
        int errnum = errno;
        delete call;
         //  Note: Should only throw EINVAL, EAGAIN, EACCES
        throw std::system_error(errnum, std::generic_category());
      } else {
        mThreadId.mId = id_receiver;
        mHandle = reinterpret_cast<HANDLE>(int_handle);
      }
    }

    bool joinable() const {return mHandle != kInvalidHandle;}

    //  Note: Due to lack of synchronization, this function has a race
    //  condition if called concurrently, which leads to undefined
    //  behavior. The same applies to all other member functions of this
    //  class, but this one is mentioned explicitly.
    void join()
    {
        using namespace std;
        if (get_id() == id(GetCurrentThreadId()))
            throw system_error(make_error_code(errc::resource_deadlock_would_occur));
        if (mHandle == kInvalidHandle)
            throw system_error(make_error_code(errc::no_such_process));
        if (!joinable())
            throw system_error(make_error_code(errc::invalid_argument));
        WaitForSingleObject(mHandle, kInfinite);
        CloseHandle(mHandle);
        mHandle = kInvalidHandle;
        mThreadId = id{};
    }

    ~thread()
    {
      if (joinable())
      {
        // @@ TODO
        /*
#ifndef NDEBUG
        std::printf("Error: Must join() or detach() a thread before \
destroying it.\n");
#endif
        */
        std::terminate();
      }
    }
    thread& operator=(const thread&) = delete;
    thread& operator=(thread&& other) noexcept
    {
      if (joinable())
      {
        // @@ TODO
        /*
#ifndef NDEBUG
        std::printf("Error: Must join() or detach() a thread before \
moving another thread to it.\n");
#endif
        */
        std::terminate();
      }
      swap(other);
      return *this;
    }
    void swap(thread& other) noexcept
    {
      std::swap(mHandle, other.mHandle);
      std::swap(mThreadId.mId, other.mThreadId.mId);
    }

    static unsigned int hardware_concurrency() noexcept
    {
      // @@ TODO: this seems like a bad idea.
      //
      /*static*/ unsigned int cached = _hardware_concurrency_helper();
      return cached;
    }

    void detach()
    {
      if (!joinable())
      {
        using namespace std;
        throw system_error(make_error_code(errc::invalid_argument));
      }
      if (mHandle != kInvalidHandle)
      {
        CloseHandle(mHandle);
        mHandle = kInvalidHandle;
      }
      mThreadId = id{};
    }
  };

  namespace detail
  {
    class ThreadIdTool
    {
    public:
      static thread::id make_id (DWORD base_id) noexcept
      {
        return thread::id(base_id);
      }
    };
  }

  namespace this_thread
  {
    inline thread::id get_id() noexcept
    {
      return detail::ThreadIdTool::make_id(GetCurrentThreadId());
    }
    inline void yield() noexcept {Sleep(0);}
    template< class Rep, class Period >
    void sleep_for( const std::chrono::duration<Rep,Period>& sleep_duration)
    {
      static constexpr DWORD kInfinite = 0xffffffffl;
      using namespace std::chrono;
      using rep = milliseconds::rep;
      rep ms = duration_cast<milliseconds>(sleep_duration).count();
      while (ms > 0)
      {
        constexpr rep kMaxRep = static_cast<rep>(kInfinite - 1);
        auto sleepTime = (ms < kMaxRep) ? ms : kMaxRep;
        Sleep(static_cast<DWORD>(sleepTime));
        ms -= sleepTime;
      }
    }
    template <class Clock, class Duration>
    void sleep_until(const std::chrono::time_point<Clock,Duration>& sleep_time)
    {
      sleep_for(sleep_time-Clock::now());
    }
  }
}

namespace std
{
  // Specialize hash for this implementation's thread::id, even if the
  // std::thread::id already has a hash.
  template<>
  struct hash<mingw_stdthread::thread::id>
  {
    typedef mingw_stdthread::thread::id argument_type;
    typedef size_t result_type;
    size_t operator() (const argument_type & i) const noexcept
    {
      return i.mId;
    }
  };
}

#endif // LIBBUTL_MINGW_THREAD_HXX
