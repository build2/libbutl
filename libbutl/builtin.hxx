// file      : libbutl/builtin.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

// On pthreads-based systems we will query the stack size of the current
// thread and use it, potentially capping to a user-specified or predefined
// value, for creating the asynchronous builtin thread.
//
#if 1 // For debugging.
#if defined(__linux__)   || \
    defined(__FreeBSD__) || \
    defined(__NetBSD__)  || \
    defined(__APPLE__)
#  include <pthread.h>
#  define LIBBUTL_BUILTIN_POSIX_THREADS
#endif
#endif

#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <memory>             // unique_ptr
#include <cassert>
#include <cstddef>            // size_t
#include <utility>            // move()
#include <cstdint>            // uint8_t
#include <functional>

#ifndef LIBBUTL_MINGW_STDTHREAD
#  include <mutex>
#  include <condition_variable>
#  ifndef LIBBUTL_BUILTIN_POSIX_THREADS
#    include <thread>
#  endif
#else
#  include <libbutl/mingw-mutex.hxx>
#  include <libbutl/mingw-thread.hxx>
#  include <libbutl/mingw-condition_variable.hxx>
#endif

#include <libbutl/path.hxx>
#include <libbutl/optional.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/timestamp.hxx>

#ifdef LIBBUTL_BUILTIN_POSIX_THREADS
#  include <libbutl/utility.hxx> // throw_system_error()
#endif

#include <libbutl/export.hxx>

namespace butl
{
  // A process/thread-like object representing a running builtin.
  //
  // For now, instead of allocating the result storage dynamically, we expect
  // it to be provided by the caller (allocating it dynamically would be
  // wasteful for synchronous builtins).
  //
  class LIBBUTL_SYMEXPORT builtin
  {
  public:
    // Wait for the builtin to complete and return its exit code. This
    // function can be called multiple times.
    //
    std::uint8_t
    wait ();

    // Return the same result as wait() if the builtin has already completed
    // and nullopt otherwise.
    //
    optional<std::uint8_t>
    try_wait ();

    // Wait for the builtin to complete for up to the specified time duration.
    // Return the same result as wait() if the builtin has completed in this
    // timeframe and nullopt otherwise.
    //
    template <typename R, typename P>
    optional<std::uint8_t>
    timed_wait (const std::chrono::duration<R, P>&);

    ~builtin () {if (state_ != nullptr) state_->join (false /* ignore_error */);}

  public:
#ifndef LIBBUTL_MINGW_STDTHREAD
    using mutex_type = std::mutex;
    using condition_variable_type = std::condition_variable;

#ifndef LIBBUTL_BUILTIN_POSIX_THREADS
    using thread_type = std::thread;
#else
    using thread_type = pthread_t;
#endif

    using unique_lock = std::unique_lock<mutex_type>;
#else
    using mutex_type = mingw_stdthread::mutex;
    using condition_variable_type = mingw_stdthread::condition_variable;
    using thread_type = mingw_stdthread::thread;

    using unique_lock = mingw_stdthread::unique_lock<mutex_type>;
#endif

    class async_state
    {
    public:
      bool finished = false;
      mutex_type mutex;
      condition_variable_type condv;

      // Note that we can't use std::function as an argument type to get rid
      // of the template since std::function can only be instantiated with a
      // copy-constructible function and that's too restrictive for us (won't
      // be able to capture auto_fd by value in a lambda, etc).
      //
      template <typename F>
      async_state (std::uint8_t&, F, optional<std::size_t> max_stack);

      // Join the thread. Throw std::system_error on the underlying OS error,
      // unless ignore_error is true. This function can be called multiple
      // times.
      //
      void
      join (bool ignore_error);

      ~async_state () {join (true /* ignore_error */);}

    private:
      template <typename F>
      void
      execute (std::uint8_t&, F&) noexcept;

#ifdef LIBBUTL_BUILTIN_POSIX_THREADS
      // Calculate the stack size for the being created thread.
      //
      static std::size_t
      stack_size (optional<std::size_t> max_stack);

      template <typename F>
      struct async_thunk
      {
        async_state* s;
        F f;
        std::uint8_t* r;

        static void*
        execute (void*) noexcept;
      };

      struct pthread_attr_deleter
      {
        void
        operator() (pthread_attr_t* a) const
        {
          int r (pthread_attr_destroy (a));

          // We should be able to destroy the valid attributes object, unless
          // something is severely damaged.
          //
          assert (r == 0);
        }
      };

      // Note that PTHREAD_NULL constant is only introduced in the
      // POSIX.1-2024 edition. Thus, let's use an additional member to track
      // whether the thread is joinable or not.
      //
      bool thread_joinable_ = false;
#endif

      thread_type thread_;
    };

    builtin (std::uint8_t& r, std::unique_ptr<async_state>&& s = nullptr)
        : result_ (r), state_ (move (s)) {}

    builtin (builtin&&) = default;

  private:
    std::uint8_t& result_;
    std::unique_ptr<async_state> state_;
  };

  // Builtin execution callbacks that can be used for checking/handling the
  // filesystem entries being acted upon (enforcing that they are sub-entries
  // of some "working" directory, registering cleanups for new entries, etc)
  // and for providing custom implementations for some functions used by
  // builtins.
  //
  // Note that the filesystem paths passed to the callbacks are absolute and
  // normalized with directories distinguished from non-directories based on
  // the lexical representation (presence of the trailing directory separator;
  // use path::to_directory() to check).
  //
  // Also note that builtins catch any exceptions that may be thrown by the
  // callbacks and, if that's the case, issue diagnostics and exit with the
  // non-zero status.
  //
  struct builtin_callbacks
  {
    // If specified, called before (pre is true) and after (pre is false) a
    // new filesystem entry is created or an existing one is re-created or
    // updated.
    //
    using create_hook = void (const path&, bool pre);

    std::function<create_hook> create;

    // If specified, called before (pre is true) and after (pre is false) a
    // filesystem entry is moved. The force argument is true if the builtin is
    // executed with the --force option.
    //
    using move_hook = void (const path& from,
                            const path& to,
                            bool force,
                            bool pre);

    std::function<move_hook> move;

    // If specified, called before (pre is true) and after (pre is false) a
    // filesystem entry is removed. The force argument is true if the builtin
    // is executed with the --force option.
    //
    using remove_hook = void (const path&, bool force, bool pre);

    std::function<remove_hook> remove;

    // If specified, called on encountering an unknown option passing the
    // argument list and the position of the option in question. Return the
    // number of parsed arguments.
    //
    using parse_option_function =
      std::size_t (const std::vector<std::string>&, std::size_t);

    std::function<parse_option_function> parse_option;

    // If specified, called by the sleep builtin instead of the default
    // implementation.
    //
    using sleep_function = void (const duration&);

    std::function<sleep_function> sleep;

    explicit
    builtin_callbacks (std::function<create_hook>           c = {},
                       std::function<move_hook>             m = {},
                       std::function<remove_hook>           r = {},
                       std::function<parse_option_function> p = {},
                       std::function<sleep_function>        s = {})
        : create (std::move (c)),
          move (std::move (m)),
          remove (std::move (r)),
          parse_option (std::move (p)),
          sleep (std::move (s)) {}

    explicit
    builtin_callbacks (std::function<sleep_function> sl)
        : sleep (std::move (sl)) {}
  };

  // Start a builtin command. Use the current process' standard streams for
  // the unopened in, out, and err file descriptors. Use the process' current
  // working directory unless an alternative is specified. Throw
  // std::system_error on failure.
  //
  // Use the max_stack argument to cap the asynchronous builtin thread stack
  // size on pthreads-based systems. Specify nullopt to use the current thread
  // stack size capped to some reasonable predefined value, if too large.
  // Specify 0 to use the current thread stack size uncapped. See build2
  // driver's --max-stack option for details on the thread stack size capping.
  //
  // Note that unlike argc/argv, args don't include the program name.
  //
  using builtin_function = builtin (std::uint8_t& result,
                                    const std::vector<std::string>& args,
                                    auto_fd in, auto_fd out, auto_fd err,
                                    const dir_path& cwd,
                                    const builtin_callbacks&,
                                    optional<std::size_t> max_stack);

  // Builtin function and weight.
  //
  // The weight between 0 and 2 reflects the builtin's contribution to the
  // containing script semantics with 0 being the lowest/ignore. Current
  // mapping is as follows:
  //
  // 0 - non-contributing (true, false)
  // 1 - non-creative     (rm, rmdir, sleep, test)
  // 2 - creative         (any builtin that may produce output)
  //
  // If the function is NULL, then the builtin has an external implementation
  // and should be executed by running the program with this name.
  //
  struct builtin_info
  {
    builtin_function* function;
    std::uint8_t      weight;
  };

  class builtin_map: public std::map<std::string, builtin_info>
  {
  public:
    using base = std::map<std::string, builtin_info>;
    using base::base;

    // Return NULL if not a builtin.
    //
    const builtin_info*
    find (const std::string&) const;
  };

  // Asynchronously run a function as if it was a builtin. The function must
  // have the std::uint8_t() signature and not throw exceptions.
  //
  // Use the max_stack argument to cap the pseudo builtin thread stack size on
  // pthreads-based systems (see builtin_function type for the details on the
  // argument semantics).
  //
  // Note that using std::function as an argument type would be too
  // restrictive (see above).
  //
  template <typename F>
  builtin
  pseudo_builtin (std::uint8_t&, F, optional<std::size_t> max_stack = nullopt);

  LIBBUTL_SYMEXPORT extern const builtin_map builtins;
}

#include <libbutl/builtin.ixx>
#include <libbutl/builtin.txx>
