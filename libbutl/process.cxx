// file      : libbutl/process.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/process.hxx>

#include <errno.h>

#ifndef _WIN32
#  include <signal.h>    // SIG*, kill()
#  include <unistd.h>    // execvp, fork, dup2, pipe, chdir, *_FILENO, getpid
#  include <sys/wait.h>  // waitpid
#  include <sys/types.h> // _stat
#  include <sys/stat.h>  // _stat(), S_IS*

// On POSIX systems we will use posix_spawn() if available and fallback to
// the less efficient fork()/exec() method otherwise.
//
// Note that using the _POSIX_SPAWN macro for detecting the posix_spawn()
// availability is unreliable. For example, unistd.h header on FreeBSD notes
// that a function related to the corresponding _POSIX_* macro may still be
// stubbed out.
//
// On Linux we will use posix_spawn() only for glibc and only starting from
// version 2.24 which introduced the Linux-specific clone() function-based
// implementation. Note that posix_spawn() in older glibc versions either uses
// "slow" fork() underneath by default, giving us no gains, or can be forced
// to use "fast" vfork(), which feels risky (see discussion threads for glibc
// bugs 378 and 10354). Also note that, at the time of this writing, glibc
// always uses fork() for systems other than Linux.
//
#  if defined(__linux__)       && \
      defined(__GLIBC__)       && \
      defined(__GLIBC_MINOR__) && \
      (__GLIBC__  > 2 || __GLIBC__ == 2 && __GLIBC_MINOR__ >= 24)
#    define LIBBUTL_POSIX_SPAWN
#    if __GLIBC__  > 2 || __GLIBC__ == 2 && __GLIBC_MINOR__ >= 29
#      define LIBBUTL_POSIX_SPAWN_CHDIR posix_spawn_file_actions_addchdir_np
#    endif
//
// On FreeBSD posix_spawn() appeared in 8.0 (see the man page for details).
//
#  elif defined(__FreeBSD__) && __FreeBSD__ >= 8
#    define LIBBUTL_POSIX_SPAWN
//
// On NetBSD posix_spawn() appeared in 6.0 (see the man page for details).
//
#  elif defined(__NetBSD__) && __NetBSD__ >= 6
#    define LIBBUTL_POSIX_SPAWN
//
// On OpenBSD posix_spawn() appeared in 5.2 (see the man page for details).
//
#  elif defined(__OpenBSD__)
#    include <sys/param.h> // OpenBSD (yyyymm)
#    if OpenBSD >= 201211  // 5.2 released on 1 Nov 2012.
#      define LIBBUTL_POSIX_SPAWN
#    endif
//
// posix_spawn() appeared in Version 3 of the Single UNIX Specification that
// was implemented in MacOS 10.5 (see the man page for details).
//
#  elif defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && \
        __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1050
#    define LIBBUTL_POSIX_SPAWN
#  endif

#  ifdef LIBBUTL_POSIX_SPAWN
#    include <spawn.h>
#  endif
#else // _WIN32
#  include <libbutl/win32-utility.hxx>

#  ifdef _MSC_VER
#    pragma warning (push, 1)
#    pragma warning (disable: 4091)
#  endif
#  include <imagehlp.h>  // ImageLoad(), etc (PE insepction for MSYS2 detect).
#  ifdef _MSC_VER
#    pragma warning (pop)
#  endif

#  include <io.h>         // _get_osfhandle(), _close()
#  include <stdlib.h>     // _MAX_PATH
#  include <sys/types.h>  // stat
#  include <sys/stat.h>   // stat(), S_IS*
#  include <processenv.h> // {Get,Free}EnvironmentStringsA()

#  ifdef _MSC_VER // Unlikely to be fixed in newer versions.
#    define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)

#    define STDIN_FILENO  0
#    define STDOUT_FILENO 1
#    define STDERR_FILENO 2
#  endif // _MSC_VER
#endif

#include <ios>      // ios_base::failure
#include <memory>   // unique_ptr
#include <cstring>  // strlen(), strchr(), strpbrk(), strncmp()
#include <utility>  // move()
#include <ostream>
#include <cassert>

#ifndef _WIN32
#  include <thread> // this_thread::sleep_for()
#else
#  include <map>
#  include <ratio>     // milli
#  include <cstdlib>   // __argv[]
#  include <algorithm> // find()
#endif

#include <libbutl/process-details.hxx>

namespace butl
{
  shared_mutex process_spawn_mutex; // Out of module purview.
}

#include <libbutl/utility.hxx>  // icasecmp()
#include <libbutl/fdstream.hxx> // fdopen_null()

using namespace std;

#ifdef _WIN32
using namespace butl::win32;
#endif

// Note that the POSIX standard requires the environ variable to be declared by
// the user, if it is used directly.
//
#ifdef LIBBUTL_POSIX_SPAWN
extern char** environ;
#endif

namespace butl
{
  // process_exit
  //
  LIBBUTL_SYMEXPORT string
  to_string (process_exit pe)
  {
    string r;

    if (pe.normal ())
    {
      r  = "exited with code ";
      r += std::to_string (pe.code ());
    }
    else
    {
      r  = "terminated abnormally: ";
      r += pe.description ();
      if (pe.core ())
        r += " (core dumped)";
    }

    return r;
  }

  // process
  //
  static process_path
  path_search (const char*, const dir_path&, bool, const char*);

  process_path process::
  path_search (const char* f, bool init,
               const dir_path& fb, bool po, const char* ps)
  {
    process_path r (try_path_search (f, init, fb, po, ps));

    if (r.empty ())
      throw process_error (ENOENT);

    return r;
  }

  process_path process::
  try_path_search (const char* f, bool init,
                   const dir_path& fb, bool po, const char* ps)
  {
    process_path r (butl::path_search (f, fb, po, ps));

    if (!init && !r.empty ())
    {
      path& rp (r.recall);
      r.initial = (rp.empty () ? (rp = path (f)) : rp).string ().c_str ();
    }

    return r;
  }

  void process::
  print (ostream& o, const char* const* args, size_t n)
  {
    size_t m (0);
    const char* const* p (args);
    do
    {
      if (m != 0)
        o << " |"; // Trailing space will be added inside the loop.

      for (m++; *p != nullptr; p++, m++)
      {
        if (p != args)
          o << ' ';

        // Quote if empty or contains spaces.
        //
        bool q (**p == '\0' || strchr (*p, ' ') != nullptr);

        if (q)
          o << '"';

        o << *p;

        if (q)
          o << '"';
      }

      if (m < n) // Can we examine the next element?
      {
        p++;
        m++;
      }

    } while (*p != nullptr);
  }

#if defined(LIBBUTL_POSIX_SPAWN) || defined(_WIN32)
  // Return true if the NULL-terminated variable list contains an (un)set of
  // the specified variable. The NULL list argument denotes an empty list.
  //
  // Note that on Windows variable names are case-insensitive.
  //
  static inline bool
  contains_envvar (const char* const* vs, const char* v, size_t n)
  {
    if (vs != nullptr)
    {
      // Note that we don't expect the number of variables to (un)set to be
      // large, so the linear search is OK.
      //
      while (const char* v1 = *vs++)
      {
#ifdef _WIN32
        if (icasecmp (v1, v, n) == 0 && (v1[n] == '=' || v1[n] == '\0'))
#else
        if (strncmp  (v1, v, n) == 0 && (v1[n] == '=' || v1[n] == '\0'))
#endif
          return true;
      }
    }

    return false;
  }
#endif

#ifndef _WIN32

  static process_path
  path_search (const char* f, const dir_path& fb, bool, const char* paths)
  {
    // Note that there is a similar version for Win32.

    using traits = path::traits_type;

    size_t fn (strlen (f));

    process_path r (f, path (), path ()); // Make sure it is not empty.
    path& rp (r.recall);
    path& ep (r.effect);

    // Check that the file exists and has at least one executable bit set.
    // This way we get a bit closer to the "continue search on EACCES"
    // semantics (see below).
    //
    auto exists = [] (const char* f) -> bool
    {
      struct stat si;
      return (stat (f, &si) == 0 &&
              S_ISREG (si.st_mode) &&
              (si.st_mode & (S_IEXEC | S_IXGRP | S_IXOTH)) != 0);
    };

    auto search = [&ep, f, fn, &exists] (const char* d,
                                         size_t dn,
                                         bool norm = false) -> bool
    {
      string s (move (ep).string ()); // Reuse buffer.

      if (dn != 0)
      {
        s.assign (d, dn);

        if (!traits::is_separator (s.back ()))
          s += traits::directory_separator;
      }

      s.append (f, fn);

      // Assume that invalid path may not refer to an existing file.
      //
      try
      {
        ep = path (move (s)); // Move back into result.

        if (norm)
          ep.normalize ();
      }
      catch (const invalid_path&)
      {
        return false;
      }

      return exists (ep.string ().c_str ());
    };

    // If there is a directory component in the file, then the PATH search
    // does not apply. If the path is relative, then prepend CWD. In both
    // cases make sure the file actually exists.
    //
    if (traits::find_separator (f, fn) != nullptr)
    {
      if (traits::absolute (f, fn))
      {
        if (exists (f))
          return r;
      }
      else
      {
        const string& d (traits::current_directory ());

        if (search (d.c_str (), d.size (), true))
          return r;
      }

      return process_path ();
    }

    // The search order is documented in exec(3). Some of the differences
    // compared to exec*p() functions:
    //
    // 1. If there no PATH, we don't default to current directory/_CS_PATH.
    // 2. We do not continue searching on EACCES from exec().
    // 3. We do not execute via default shell on ENOEXEC from exec().
    //
    optional<string> paths_env;
    if (paths == nullptr && (paths_env = getenv ("PATH")))
      paths = paths_env->c_str ();

    for (const char* b (paths), *e;
         b != nullptr;
         b = (e != nullptr ? e + 1 : e))
    {
      e = strchr (b, traits::path_separator);

      // Empty path (i.e., a double colon or a colon at the beginning or end
      // of PATH) means search in the current directory.
      //
      if (search (b, e != nullptr ? e - b : strlen (b)))
        return r;
    }

    // If we were given a fallback, try that.
    //
    if (!fb.empty ())
    {
      if (search (fb.string ().c_str (), fb.string ().size ()))
      {
        // In this case we have to set the recall path. And we know from
        // search() implementation that it will be the same as effective.
        // Which means we can just move effective to recall.
        //
        rp.swap (ep);

        return r;
      }
    }

    // Did not find anything.
    //
    return process_path ();
  }

  process::
  process (const process_path& pp, const char* const* args,
           pipe pin, pipe pout, pipe perr,
           const char* cwd,
           const char* const* evars)
  {
    int in  (pin.in);
    int out (pout.out);
    int err (perr.out);

    fdpipe out_fd;
    fdpipe in_ofd;
    fdpipe in_efd;

    auto open_pipe = [] () -> fdpipe
    {
      try
      {
        return fdopen_pipe ();
      }
      catch (const ios_base::failure&)
      {
        // Translate to process_error.
        //
        // For old versions of g++ (as of 4.9) ios_base::failure is not derived
        // from system_error and so we cannot recover the errno value. On the
        // other hand the only possible values are EMFILE and ENFILE. Lets use
        // EMFILE as the more probable. This is a temporary code after all.
        //
        throw process_error (EMFILE);
      }
    };

    auto open_null = [] () -> auto_fd
    {
      try
      {
        return fdopen_null ();
      }
      catch (const ios_base::failure& e)
      {
        // Translate to process_error.
        //
        // For old versions of g++ (as of 4.9) ios_base::failure is not derived
        // from system_error and so we cannot recover the errno value. Lets use
        // EIO in this case. This is a temporary code after all.
        //
        const system_error* se (dynamic_cast<const system_error*> (&e));

        throw process_error (se != nullptr
                             ? se->code ().value ()
                             : EIO);
      }
    };

    // If we are asked to open null (-2) then open "half-pipe".
    //
    if (in == -1)
      out_fd = open_pipe ();
    else if (in == -2)
      out_fd.in = open_null ();

    if (out == -1)
      in_ofd = open_pipe ();
    else if (out == -2)
      in_ofd.out = open_null ();

    if (err == -1)
      in_efd = open_pipe ();
    else if (err == -2)
      in_efd.out = open_null ();

    // If there is no user-supplied CWD and we have thread-specific override,
    // use that instead of defaulting to the process-wide value.
    //
    if (cwd == nullptr || *cwd == '\0')
    {
      if (const string* twd = path::traits_type::thread_current_directory ())
        cwd = twd->c_str ();
    }

    const char* const* tevars (thread_env ());

    // The posix_spawn()-based implementation.
    //
#ifdef LIBBUTL_POSIX_SPAWN
#ifndef LIBBUTL_POSIX_SPAWN_CHDIR
    if (cwd == nullptr || *cwd == '\0') // Not changing CWD.
#endif
    {
      auto fail = [] (int error) {throw process_error (error);};

      // Setup the file-related actions to be performed in the child process.
      // Note that the actions will be performed in the order they are set up.
      //
      posix_spawn_file_actions_t fa;
      int r (posix_spawn_file_actions_init (&fa));
      if (r != 0)
        fail (r);

      unique_ptr<posix_spawn_file_actions_t,
                 void (*)(posix_spawn_file_actions_t*)> deleter (
        &fa,
        [] (posix_spawn_file_actions_t* fa)
        {
          int r (posix_spawn_file_actions_destroy (fa));

          // The only possible error for posix_spawn_file_actions_destroy() is
          // EINVAL, which shouldn't happen unless something is severely
          // broken.
          //
          assert (r == 0);
        });

      // Redirect the standard streams.
      //
      // Note that here we convert the child process housekeeping actions for
      // the fork-based implementation (see below) into the file action
      // sequence for the posix_spawn() call.
      //
      auto duplicate = [&fa, &fail] (int sd, int fd, fdpipe& pd)
      {
        if (fd == -1 || fd == -2)
          fd = (sd == STDIN_FILENO ? pd.in : pd.out).get ();

        assert (fd > -1);

        int r (posix_spawn_file_actions_adddup2 (&fa, fd, sd));
        if (r != 0)
          fail (r);

        auto reset = [&fa, &fail] (const auto_fd& fd)
        {
          int r;
          if (fd != nullfd &&
              (r = posix_spawn_file_actions_addclose (&fa, fd.get ())) != 0)
            fail (r);
        };

        reset (pd.in);
        reset (pd.out);
      };

      if (in != STDIN_FILENO)
        duplicate (STDIN_FILENO, in, out_fd);

      if (out == STDERR_FILENO)
      {
        if (err != STDERR_FILENO)
          duplicate (STDERR_FILENO, err, in_efd);

        duplicate (STDOUT_FILENO, out, in_ofd);
      }
      else
      {
        if (out != STDOUT_FILENO)
          duplicate (STDOUT_FILENO, out, in_ofd);

        if (err != STDERR_FILENO)
          duplicate (STDERR_FILENO, err, in_efd);
      }

      // Change the current working directory if requested.
      //
#ifdef LIBBUTL_POSIX_SPAWN_CHDIR
      if (cwd != nullptr &&
          *cwd != '\0'   &&
          (r = LIBBUTL_POSIX_SPAWN_CHDIR (&fa, cwd)) != 0)
        fail (r);
#endif

      // Set/unset the child process environment variables if requested.
      //
      vector<const char*> new_env;

      if (tevars != nullptr || evars != nullptr)
      {
        // Copy the non-overridden process environment variables into the
        // child's environment.
        //
        for (const char* const* ev (environ); *ev != nullptr; ++ev)
        {
          const char* v (*ev);
          const char* e (strchr (v, '='));
          size_t n (e != nullptr ? e - v : strlen (v));

          if (!contains_envvar (tevars, v, n) &&
              !contains_envvar (evars, v, n))
            new_env.push_back (v);
        }

        // Copy non-overridden variable assignments into the child's
        // environment.
        //
        auto set_vars = [&new_env] (const char* const* vs,
                                    const char* const* ovs = nullptr)
        {
          if (vs != nullptr)
          {
            while (const char* v = *vs++)
            {
              const char* e (strchr (v, '='));
              if (e != nullptr && !contains_envvar (ovs, v, e - v))
                new_env.push_back (v);
            }
          }
        };

        set_vars (tevars, evars);
        set_vars (evars);

        new_env.push_back (nullptr);
      }

      ulock l (process_spawn_mutex); // Note: won't be released in child.

      // Note that in most non-fork based implementations this call suspends
      // the parent thread until the child process calls exec() or terminates.
      // This avoids "text file busy" issue (see the fork-based code below):
      // due to the process_spawn_mutex lock the execution of the script is
      // delayed until the child closes the descriptor.
      //
      r = posix_spawn (&handle,
                       pp.effect_string (),
                       &fa,
                       nullptr /* attrp */,
                       const_cast<char* const*> (&args[0]),
                       new_env.empty ()
                       ? environ
                       : const_cast<char* const*> (new_env.data ()));
        if (r != 0)
          fail (r);
    } // Release the lock in parent.
#ifndef LIBBUTL_POSIX_SPAWN_CHDIR
    else
#endif
#endif // LIBBUTL_POSIX_SPAWN

    // The fork-based implementation.
    //
#ifndef LIBBUTL_POSIX_SPAWN_CHDIR
    {
      auto fail = [] (bool child)
      {
        if (child)
          throw process_child_error (errno);
        else
          throw process_error (errno);
      };

      ulock l (process_spawn_mutex); // Will not be released in child.

      // Note that the file descriptors with the FD_CLOEXEC flag stay open in
      // the child process between fork() and exec() calls. This may cause the
      // "text file busy" issue: if some other thread creates a shell script
      // and the write-open file descriptor leaks into some child process,
      // then exec() for this script fails with ETXTBSY (see exec() man page
      // for details). If that's the case, it feels like such a descriptor
      // should not stay open for too long. Thus, we will retry the exec()
      // calls for about half a second.
      //
      handle = fork ();

      if (handle == -1)
        fail (false /* child */);

      if (handle == 0)
      {
        // Child.
        //
        // NOTE: make sure not to call anything that may acquire a mutex that
        //       could be already acquired in another thread, most notably
        //       malloc(). @@ What about exceptions (all the fail() calls)?

        // Duplicate the user-supplied (fd > -1) or the created pipe descriptor
        // to the standard stream descriptor (read end for STDIN_FILENO, write
        // end otherwise). Close the pipe afterwards.
        //
        auto duplicate = [&fail] (int sd, int fd, fdpipe& pd)
        {
          if (fd == -1 || fd == -2)
            fd = (sd == STDIN_FILENO ? pd.in : pd.out).get ();

          assert (fd > -1);

          if (dup2 (fd, sd) == -1)
            fail (true /* child */);

          pd.in.reset ();  // Silently close.
          pd.out.reset (); // Silently close.
        };

        if (in != STDIN_FILENO)
          duplicate (STDIN_FILENO, in, out_fd);

        // If stdout is redirected to stderr (out == 2) we need to duplicate it
        // after duplicating stderr to pickup the proper fd. Otherwise keep the
        // "natual" order of duplicate() calls, so if stderr is redirected to
        // stdout it picks up the proper fd as well.
        //
        if (out == STDERR_FILENO)
        {
          if (err != STDERR_FILENO)
            duplicate (STDERR_FILENO, err, in_efd);

          duplicate (STDOUT_FILENO, out, in_ofd);
        }
        else
        {
          if (out != STDOUT_FILENO)
            duplicate (STDOUT_FILENO, out, in_ofd);

          if (err != STDERR_FILENO)
            duplicate (STDERR_FILENO, err, in_efd);
        }

        // Change current working directory if requested.
        //
        if (cwd != nullptr && *cwd != '\0' && chdir (cwd) != 0)
          fail (true /* child */);

        // Set/unset environment variables.
        //
        auto set_vars = [] (const char* const* vs)
        {
          if (vs != nullptr)
          {
            while (const char* v = *vs++)
            {
              const char* e (strchr (v, '='));

              try
              {
                // @@ TODO: redo without allocation (PATH_MAX?) Maybe
                //          also using C API to avoid exceptions.
                //
                if (e != nullptr)
                  setenv (string (v, e - v), e + 1);
                else
                  unsetenv (v);
              }
              catch (const system_error& e)
              {
                // @@ Should we assume this cannot throw?
                //
                throw process_child_error (e.code ().value ());
              }
            }
          }
        };

        set_vars (tevars);
        set_vars (evars);

        // Try to re-exec after the "text file busy" failure for 450ms.
        //
        // Note that execv() does not return on success.
        //
        for (size_t i (1); i != 10; ++i)
        {
          execv (pp.effect_string (), const_cast<char**> (&args[0]));

          if (errno != ETXTBSY)
            break;

          this_thread::sleep_for (i * 10ms);
        }

        fail (true /* child */);
      }
    } // Release the lock in parent.
#endif // LIBBUTL_POSIX_SPAWN_CHDIR

    assert (handle != 0); // Shouldn't get here unless in the parent process.

    this->out_fd = move (out_fd.out);
    this->in_ofd = move (in_ofd.in);
    this->in_efd = move (in_efd.in);
  }

  bool process::
  wait (bool ie)
  {
    if (handle != 0)
    {
      // First close any open pipe ends for good measure but ignore any
      // errors.
      //
      out_fd.reset ();
      in_ofd.reset ();
      in_efd.reset ();

      int es;
      int r (waitpid (handle, &es, 0));
      handle = 0; // We have tried.

      if (r == -1)
      {
        // If ignore errors then just leave exit nullopt, so it has "no exit
        // information available" semantics.
        //
        if (!ie)
          throw process_error (errno);
      }
      else
        exit = process_exit (es, process_exit::as_status);
    }

    return exit && exit->normal () && exit->code () == 0;
  }

  optional<bool> process::
  try_wait ()
  {
    if (handle != 0)
    {
      int es;
      int r (waitpid (handle, &es, WNOHANG));

      if (r == 0) // Not exited yet.
        return nullopt;

      handle = 0; // We have tried.

      if (r == -1)
        throw process_error (errno);

      exit = process_exit (es, process_exit::as_status);
    }

    return exit ? static_cast<bool> (*exit) : optional<bool> ();
  }

  template <>
  optional<bool> process::
  timed_wait (const chrono::milliseconds& tm)
  {
    using namespace chrono;

    // On POSIX this seems to be the best way for multi-threaded processes.
    //
    const milliseconds sd (10);
    for (milliseconds d (tm); !try_wait (); d -= sd)
    {
      this_thread::sleep_for (d < sd ? d : sd);

      if (d < sd)
        break;
    }

    return try_wait ();
  }

  void process::
  kill ()
  {
    if (handle != 0 && ::kill (handle, SIGKILL) == -1)
      throw process_error (errno);
  }

  void process::
  term ()
  {
    if (handle != 0 && ::kill (handle, SIGTERM) == -1)
      throw process_error (errno);
  }

  process::id_type process::
  current_id ()
  {
    return getpid ();
  }

  process::handle_type process::
  current_handle ()
  {
    return getpid ();
  }

  // process_exit
  //
  process_exit::
  process_exit (code_type c)
      //
      // Note that such an initialization is not portable as POSIX doesn't
      // specify the bits layout for the value returned by waitpid(). However
      // for the major POSIX systems (Linux, FreeBSD, MacOS) it is the
      // following:
      //
      // [0,  7) - terminating signal
      // [7,  8) - coredump flag
      // [8, 16) - program exit code
      //
      // Also the lowest 7 bits value is used to distinguish the normal and
      // abnormal process terminations. If it is zero then the program exited
      // normally and the exit code is available.
      //
      : status (c << 8)
  {
  }

  // Make sure the bits layout we stick to (read above) correlates to the W*()
  // macros implementations for the current platform.
  //
  namespace details
  {
    // W* macros may require an argument to be lvalue (for example for glibc).
    //
    static const process_exit::status_type status_code (0xFF00);

    // On Cygwin/MSYS WIFEXITED() is not constexpr. So we will just hope
    // of the best.
    //
#if !defined(__MSYS__) && !defined(__CYGWIN__)
    static_assert (WIFEXITED    (status_code) &&
                   WEXITSTATUS  (status_code) == 0xFF &&
                   !WIFSIGNALED (status_code),
                   "unexpected process exit status bits layout");
#endif
  }

  bool process_exit::
  normal () const
  {
    return WIFEXITED (status);
  }

  process_exit::code_type process_exit::
  code () const
  {
    assert (normal ());
    return WEXITSTATUS (status);
  }

  int process_exit::
  signal () const
  {
    assert (!normal ());

    // WEXITSTATUS() and WIFSIGNALED() can both return false for the same
    // status, so we have neither exit code nor signal. We return zero for
    // such a case.
    //
    return WIFSIGNALED (status) ? WTERMSIG (status) : 0;
  }

  bool process_exit::
  core () const
  {
    assert (!normal ());

    // Not a POSIX macro (available on Linux, FreeBSD, MacOS).
    //
#ifdef WCOREDUMP
    return WIFSIGNALED (status) && WCOREDUMP (status);
#else
    return false;
#endif
  }

  string process_exit::
  description () const
  {
    assert (!normal ());

    // It would be convenient to use strsignal() or sys_siglist[] to obtain a
    // signal name for the number, but the function is not thread-safe and the
    // array is not POSIX. So we will use the custom mapping of POSIX signals
    // (IEEE Std 1003.1-2008, 2016 Edition) to their names (as they appear in
    // glibc).
    //
    switch (signal ())
    {
    case SIGHUP:    return "hangup (SIGHUP)";
    case SIGINT:    return "interrupt (SIGINT)";
    case SIGQUIT:   return "quit (SIGQUIT)";
    case SIGILL:    return "illegal instruction (SIGILL)";
    case SIGABRT:   return "aborted (SIGABRT)";
    case SIGFPE:    return "floating point exception (SIGFPE)";
    case SIGKILL:   return "killed (SIGKILL)";
    case SIGSEGV:   return "segmentation fault (SIGSEGV)";
    case SIGPIPE:   return "broken pipe (SIGPIPE)";
    case SIGALRM:   return "alarm clock (SIGALRM)";
    case SIGTERM:   return "terminated (SIGTERM)";
    case SIGUSR1:   return "user defined signal 1 (SIGUSR1)";
    case SIGUSR2:   return "user defined signal 2 (SIGUSR2)";
    case SIGCHLD:   return "child exited (SIGCHLD)";
    case SIGCONT:   return "continued (SIGCONT)";
    case SIGSTOP:   return "stopped (process; SIGSTOP)";
    case SIGTSTP:   return "stopped (typed at terminal; SIGTSTP)";
    case SIGTTIN:   return "stopped (tty input; SIGTTIN)";
    case SIGTTOU:   return "stopped (tty output; SIGTTOU)";
    case SIGBUS:    return "bus error (SIGBUS)";

    // Unavailabe on MacOS 10.11.
    //
#ifdef SIGPOLL
    case SIGPOLL:   return "I/O possible (SIGPOLL)";
#endif

    case SIGPROF:   return "profiling timer expired (SIGPROF)";
    case SIGSYS:    return "bad system call (SIGSYS)";
    case SIGTRAP:   return "trace/breakpoint trap (SIGTRAP)";
    case SIGURG:    return "urgent I/O condition (SIGURG)";
    case SIGVTALRM: return "virtual timer expired (SIGVTALRM)";
    case SIGXCPU:   return "CPU time limit exceeded (SIGXCPU)";
    case SIGXFSZ:   return "file size limit exceeded (SIGXFSZ)";

    case 0:         return "status unknown";
    default:        return "unknown signal " + std::to_string (signal ());
    }
  }

#else // _WIN32

  static process_path
  path_search (const char* f, const dir_path& fb, bool po, const char* paths)
  {
    // Note that there is a similar version for Win32.

    typedef path::traits_type traits;

    size_t fn (strlen (f));

    // Unless there is already the .exe/.bat extension, then we will need to
    // add it.
    //
    bool ext;
    {
      const char* e (traits::find_extension (f, fn));
      ext = (e == nullptr ||
             (icasecmp (e, ".exe") != 0 &&
              icasecmp (e, ".bat") != 0 &&
              icasecmp (e, ".cmd") != 0));
    }

    process_path r (f, path (), path ()); // Make sure it is not empty.
    path& rp (r.recall);
    path& ep (r.effect);

    // Check that the file exists. Since the executable mode is set according
    // to the file extension, we don't check for that.
    //
    auto exists = [] (const char* f) -> bool
    {
      struct _stat si;
      return _stat (f, &si) == 0 && S_ISREG (si.st_mode);
    };

    // Check with extensions: .exe, .cmd, and .bat.
    //
    auto exists_ext = [&exists] (string& s) -> bool
    {
      size_t i (s.size () + 1); // First extension letter.

      s += ".exe";
      if (exists (s.c_str ()))
        return true;

      s[i] = 'c'; s[i + 1] = 'm'; s[i + 2] = 'd';
      if (exists (s.c_str ()))
        return true;

      s[i] = 'b'; s[i + 1] = 'a'; s[i + 2] = 't';
      return exists (s.c_str ());
    };

    auto search = [&ep, f, fn, ext, &exists, &exists_ext] (
      const char* d, size_t dn, bool norm = false) -> bool
    {
      string s (move (ep).string ()); // Reuse buffer.

      if (dn != 0)
      {
        s.assign (d, dn);

        if (!traits::is_separator (s.back ()))
          s += traits::directory_separator;
      }

      s.append (f, fn);
      ep = path (move (s)); // Move back into result.

      if (norm)
        ep.normalize ();

      if (!ext)
        return exists (ep.string ().c_str ());

      // Try with the extensions.
      //
      s = move (ep).string ();
      bool e (exists_ext (s));
      ep = path (move (s));
      return e;
    };

    // If there is a directory component in the file, then the PATH search
    // does not apply. If the path is relative, then prepend CWD. In both
    // cases we may still need to append the extension and make sure the file
    // actually exists.
    //
    if (traits::find_separator (f, fn) != nullptr)
    {
      if (traits::absolute (f, fn))
      {
        bool e;
        if (!ext)
          e = exists (r.effect_string ());
        else
        {
          string s (f, fn);
          e = exists_ext (s);
          ep = path (move (s));
        }

        if (e)
          return r;
      }
      else
      {
        const string& d (traits::current_directory ());

        if (search (d.c_str (), d.size (), true)) // Appends extension.
          return r;
      }

      return process_path ();
    }

    // The search order is documented in CreateProcess(). First we look in the
    // directory of the parent executable.
    //
    if (!po)
    {
      char d[_MAX_PATH + 1];
      DWORD n (GetModuleFileName (NULL, d, _MAX_PATH + 1));

      if (n == 0 || n == _MAX_PATH + 1) // Failed or truncated.
        throw process_error (last_error_msg ());

      const char* p (traits::rfind_separator (d, n));
      assert (p != nullptr);

      if (search (d, p - d + 1)) // Include trailing slash.
      {
        // In this case we have to set the recall path.
        //
        // Note that the directory we have extracted is always absolute but
        // the parent's recall path (argv[0]) might be relative. It seems,
        // ideally, we would want to use parent's argv[0] dir (if any) to form
        // the recall path. In particular, if the parent has no directory,
        // then it means it was found via the standard search (e.g., PATH) and
        // then so should the child.
        //
        // How do we get the parent's argv[0]? Luckily, here is __argv on
        // Windows.
        //
        const char* d (__argv[0]);
        if (const char* p = traits::rfind_separator (d))
        {
          string s (d, p - d + 1); // Include trailing slash.
          s.append (f, fn);
          rp = path (move (s));

          // If recall is the same as effective, then set effective to empty.
          //
          if (rp == ep)
            ep.clear ();
        }

        return r;
      }
    }

    // Next look in the current working directory. Crazy, I know.
    //
    // The recall path is the same as initial, though it might not be a bad
    // idea to prepend .\ for clarity.
    //
    if (!po)
    {
      const string& d (traits::current_directory ());

      if (search (d.c_str (), d.size ()))
        return r;
    }

    // Now search in PATH. Recall is unchanged.
    //
    optional<string> paths_env;
    if (paths == nullptr && (paths_env = getenv ("PATH")))
      paths = paths_env->c_str ();

    for (const char* b (paths), *e;
         b != nullptr;
         b = (e != nullptr ? e + 1 : e))
    {
      e = strchr (b, traits::path_separator);

      // Empty path (i.e., a double colon or a colon at the beginning or end
      // of PATH) means search in the current directory. Silently skip invalid
      // paths.
      //
      try
      {
        if (search (b, e != nullptr ? e - b : strlen (b)))
          return r;
      }
      catch (const invalid_path&)
      {
      }
    }

    // Finally, if we were given a fallback, try that. This case is similar to
    // searching in the parent executable's directory.
    //
    if (!fb.empty ())
    {
      // I would have been nice to preserve trailing slash (by using
      // representation() instead of string()), but that would involve a
      // copy. Oh, well, can't always win.
      //
      if (search (fb.string ().c_str (), fb.string ().size ()))
      {
        // In this case we have to set the recall path. At least here we got
        // to keep the original slash.
        //
        rp = fb;
        rp /= f;

        // If recall is the same as effective, then set effective to empty.
        //
        if (rp == ep)
          ep.clear ();

        return r;
      }
    }

    // Did not find anything.
    //
    return process_path ();
  }

  // Make handles inheritable. The process_spawn_mutex must be pre-acquired for
  // exclusive access. Revert handles inheritability state in destructor.
  //
  // There is a period of time when the process ctor makes file handles it
  // passes to the child to be inheritable, that otherwise are not inheritable
  // by default. During this time these handles can also be inherited by other
  // (irrelevant) child processed spawned from other threads. That can lead to
  // some unwanted consequences, such as inability to delete a file
  // corresponding to such a handle until all childs, that the handle leaked
  // into, terminate. To prevent this behavior the specific sequence of steps
  // (that involves making handles inheritable, spawning process and reverting
  // handles to non-inheritable state back) will be performed after aquiring
  // the process_spawn_mutex (that is released afterwards).
  //
  class inheritability_lock
  {
  public:
    // Require the proof that the mutex is pre-acquired for exclusive access.
    // Create locked.
    //
    inheritability_lock (const ulock&) {}

    ~inheritability_lock ()
    {
      unlock (); // Can't throw.
    }

    void
    inheritable (HANDLE h)
    {
      assert (locked_);

      inheritable (h, true);  // Can throw.
      handles_.push_back (h);
    }

    void
    unlock ()
    {
      if (locked_)
      {
        inheritable (false);
        locked_ = false;
      }
    }

    void
    lock ()
    {
      if (!locked_)
      {
        inheritable (true);
        locked_ = true;
      }
    }

  private:
    void
    inheritable (HANDLE h, bool state)
    {
      if (!SetHandleInformation (
            h, HANDLE_FLAG_INHERIT, state ? HANDLE_FLAG_INHERIT : 0))
      {
        if (state)
          throw process_error (last_error_msg ());

        // We should be able to successfully reset the HANDLE_FLAG_INHERIT flag
        // that we successfully set, unless something is severely damaged.
        //
        assert (false);
      }
    }

    void
    inheritable (bool state)
    {
      for (auto h: handles_)
        inheritable (h, state);
    }

  private:
    small_vector<HANDLE, 3> handles_;
    bool locked_ = true;
  };

  const char* process::
  quote_argument (const char* a, string& s, bool bat)
  {
    // On Windows we need to protect values with spaces using quotes. Since
    // there could be actual quotes in the value, we need to escape them.
    //
    // For batch files we also protect equal (`=`), comma (`,`) and semicolon
    // (`;`) since otherwise an argument containing any of these will be split
    // into several as if they were spaces (that is, the parts will appear in
    // %1 %2, etc., instead of all in %1). This of course could break some
    // batch files that rely on this semantics (for example, to automatically
    // handle --foo=bar as --foo bar) but overall seeing a single argument
    // (albeit quoted) is closer to the behavior of real executables. So we do
    // this by default and if it becomes a problem we can invent a flag
    // (probably in process_env) to disable this quoting (and while at it we
    // may add a flag to disable all quoting since the user may need to quote
    // some arguments but not others).
    //
    // While `()` and `[]` are not special characters, some "subsystems"
    // (e.g., Cygwin/MSYS2) try to interpret them in certain contexts (e.g.,
    // relative paths). So we quote them as well (over-quoting seems to be
    // harmless according to the "Parsing C Command-Line Arguments" MSDN
    // article).
    //
    bool q (*a == '\0' || strpbrk (a, bat ? " =,;" : " ()[]") != nullptr);

    if (!q && strchr (a, '"') == nullptr)
      return a;

    s.clear ();

    if (q)
      s += '"';

    // Note that backslashes don't need escaping, unless they immediately
    // precede the double quote (see "Parsing C Command-Line Arguments" MSDN
    // article for details). For example:
    //
    // -DPATH="C:\\foo\\"  ->  -DPATH=\"C:\\foo\\\\\"
    // -DPATH=C:\foo bar\  ->  "-DPATH=C:\foo bar\\"
    //
    size_t nbs (0); // Number of consecutive backslashes.
    for (size_t i (0), n (strlen (a)); i != n; ++i)
    {
      char c (a[i]);

      if (c != '"')
        s += c;
      else
      {
        if (nbs != 0)
          s.append (nbs, '\\'); // Escape backslashes.

        s += "\\\"";            // Escape quote.
      }

      nbs = c == '\\' ? nbs + 1 : 0;
    }

    if (q)
    {
      if (nbs != 0)
        s.append (nbs, '\\'); // Escape backslashes.

      s += '"';
    }

    return s.c_str ();
  }

  // Protected by process_spawn_mutex. See below for the gory details.
  //
  static map<string, bool> detect_msys_cache_;

  process::
  process (const process_path& pp, const char* const* args,
           pipe pin, pipe pout, pipe perr,
           const char* cwd,
           const char* const* evars)
  {
    int in  (pin.in);
    int out (pout.out);
    int err (perr.out);

    auto fail = [] (const char* m = nullptr)
    {
      throw process_error (m == nullptr ? last_error_msg () : m);
    };

    // If there is no user-supplied CWD and we have thread-specific override,
    // use that instead of defaulting to the process-wide value.
    //
    if (cwd == nullptr || *cwd == '\0')
    {
      if (const string* twd = path::traits_type::thread_current_directory ())
        cwd = twd->c_str ();
    }

    // (Un)set the environment variables for the child process.
    //
    // Note that we can not do it incrementally, as for POSIX implementation.
    // Instead we need to create the full environment block and pass it to the
    // CreateProcess() function call. So we will copy variables from the
    // environment block of the current process, but only those that are not
    // requested to be (un)set. After that we will additionally copy variables
    // that need to be set.
    //
    vector<char> new_env;

    const char* const* tevars (thread_env ());

    if (tevars != nullptr || evars != nullptr)
    {
      // The environment block contains the variables in the following format:
      //
      // name=value\0...\0
      //
      // Note the trailing NULL character that follows the last variable
      // (null-terminated) string.
      //
      unique_ptr<char, void (*)(char*)> pevars (
        GetEnvironmentStringsA (),
        [] (char* p)
        {
          // We should be able to successfully free the valid environment
          // variables memory block, unless something is severely damaged.
          //
          if (p != nullptr && !FreeEnvironmentStringsA (p))
            assert (false);
        });

      if (pevars.get () == nullptr)
        fail ();

      // Copy the non-overridden process environment variables into the
      // child's environment.
      //
      for (const char* v (pevars.get ()); *v != '\0'; )
      {
        size_t n (strlen (v) + 1); // Includes NULL character.

        const char* e (strchr (v, '='));
        size_t nn (e != nullptr ? e - v : n - 1);

        if (!contains_envvar (tevars, v, nn) &&
            !contains_envvar (evars, v, nn))
          new_env.insert (new_env.end (), v, v + n);

        v += n;
      }

      // Copy non-overridden variable assignments into the child's
      // environment.
      //
      auto set_vars = [&new_env] (const char* const* vs,
                                  const char* const* ovs = nullptr)
      {
        if (vs != nullptr)
        {
          while (const char* v = *vs++)
          {
            const char* e (strchr (v, '='));
            if (e != nullptr && !contains_envvar (ovs, v, e - v))
              new_env.insert (new_env.end (), v, v + strlen (v) + 1);
          }
        }
      };

      set_vars (tevars, evars);
      set_vars (evars);

      new_env.push_back ('\0'); // Terminate the new environment block.
    }

    // Figure out if this is a batch file since running them requires starting
    // cmd.exe and passing the batch file as an argument (see CreateProcess()
    // for deails).
    //
    optional<string> batch;
    {
      const char* p (pp.effect_string ());
      const char* e (path::traits_type::find_extension (p));
      if (e != nullptr && (icasecmp (e, ".bat") == 0 ||
                           icasecmp (e, ".cmd") == 0))
      {
        batch = getenv ("COMSPEC");

        if (!batch)
          batch = "C:\\Windows\\System32\\cmd.exe";
      }
    }

    fdpipe out_fd;
    fdpipe in_ofd;
    fdpipe in_efd;

    auto open_pipe = [] () -> fdpipe
    {
      try
      {
        return fdopen_pipe ();
      }
      catch (const ios_base::failure&)
      {
        // Translate to process_error.
        //
        // For old versions of g++ (as of 4.9) ios_base::failure is not derived
        // from system_error and so we cannot recover the errno value. On the
        // other hand the only possible values are EMFILE and ENFILE. Lets use
        // EMFILE as the more probable. Also let's make no distinction for VC.
        // This is a temporary code after all.
        //
        throw process_error (EMFILE);
      }
    };

    auto open_null = [] () -> auto_fd
    {
      // Note that we are using a faster, temporary file-based emulation of
      // NUL since we have no way of making sure the child buffers things
      // properly (and by default they seem no to).
      //
      try
      {
        return fdopen_null (true /* temp */);
      }
      catch (const ios_base::failure& e)
      {
        // Translate to process_error.
        //
        // For old versions of g++ (as of 4.9) ios_base::failure is not derived
        // from system_error and so we cannot recover the errno value. Lets use
        // EIO in this case. This is a temporary code after all.
        //
        const system_error* se (dynamic_cast<const system_error*> (&e));

        throw process_error (se != nullptr
                             ? se->code ().value ()
                             : EIO);
      }
    };

    // If we are asked to open null (-2) then open "half-pipe".
    //
    if (in == -1)
      out_fd = open_pipe ();
    else if (in == -2)
      out_fd.in = open_null ();

    if (out == -1)
      in_ofd = open_pipe ();
    else if (out == -2)
      in_ofd.out = open_null ();

    if (err == -1)
      in_efd = open_pipe ();
    else if (err == -2)
      in_efd.out = open_null ();

    // Create the process.
    //

    // Serialize the arguments to string.
    //
    string cmd_line;
    {
      auto append = [&batch, &cmd_line, buf = string ()] (const char* a) mutable
      {
        if (!cmd_line.empty ())
          cmd_line += ' ';

        cmd_line += quote_argument (a, buf, batch.has_value ());
      };

      if (batch)
      {
        append (batch->c_str ());
        append ("/c");
        append (pp.effect_string ());
      }

      for (const char* const* p (args + (batch ? 1 : 0));
           *p != 0;
           ++p)
        append (*p);
    }

    // Prepare other process information.
    //
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    memset (&si, 0, sizeof (STARTUPINFO));
    memset (&pi, 0, sizeof (PROCESS_INFORMATION));

    si.cb = sizeof (STARTUPINFO);
    si.dwFlags |= STARTF_USESTDHANDLES;

    {
      ulock l (process_spawn_mutex);
      inheritability_lock il (l);

      // Resolve file descriptor to HANDLE and make sure it is inherited (if
      // requested). Note that the handle is closed either when CloseHandle()
      // is called for it or when _close() is called for the associated file
      // descriptor. Make sure that either the original file descriptor or the
      // resulting HANDLE is closed but not both of them.
      //
      auto get_osfhandle = [&fail, &il] (int fd, bool inherit = true) -> HANDLE
      {
        HANDLE h (reinterpret_cast<HANDLE> (_get_osfhandle (fd)));
        if (h == INVALID_HANDLE_VALUE)
          fail ("unable to obtain file handle");

        // Make the handle inheritable by the child unless it is already
        // inheritable.
        //
        if (inherit)
        {
          DWORD f;
          if (!GetHandleInformation (h, &f))
            fail ();

          // Note that the flag check is essential as SetHandleInformation()
          // fails for standard handles and their duplicates.
          //
          if ((f & HANDLE_FLAG_INHERIT) == 0)
            il.inheritable (h);
        }

        return h;
      };

      si.hStdInput = in == -1 || in == -2
        ? get_osfhandle (out_fd.in.get ())
        : (in == STDIN_FILENO
           ? GetStdHandle (STD_INPUT_HANDLE)
           : get_osfhandle (in));

      si.hStdOutput = out == -1 || out == -2
        ? get_osfhandle (in_ofd.out.get ())
        : (out == STDOUT_FILENO
           ? GetStdHandle (STD_OUTPUT_HANDLE)
           : get_osfhandle (out));

      si.hStdError = err == -1 || err == -2
        ? get_osfhandle (in_efd.out.get ())
        : (err == STDERR_FILENO
           ? GetStdHandle (STD_ERROR_HANDLE)
           : get_osfhandle (err));

      // Perform standard stream redirection if requested.
      //
      if (err == STDOUT_FILENO)
        si.hStdError = si.hStdOutput;
      else if (out == STDERR_FILENO)
        si.hStdOutput = si.hStdError;

      if (err == STDIN_FILENO ||
          out == STDIN_FILENO ||
          in == STDOUT_FILENO ||
          in == STDERR_FILENO)
        fail ("invalid file descriptor");

      // Ready for some "Fun with Windows"(TM)? Here is what's in today's
      // episode: MSYS2 (actually, Cygwin) tries to emulate POSIX fork() on
      // Win32 via some pretty heavy hackery. As a result it makes a bunch of
      // assumptions such as that the child process will have the same virtual
      // memory position as the parent and that nobody interferes in its
      // child-parent dance.
      //
      // This, however, doesn't always pan out: for reasons unknown Windows
      // sometimes decides to start the child somewhere else (or, as Cygwin
      // FAQ puts it: "sometimes Windows sets up a process environment that is
      // even more hostile to fork() than usual"). Also things like Windows
      // Defender (collectively called Big List Of Dodgy Apps/BLODA in Cygwin
      // speak) do interfere in all kinds of ways.
      //
      // We also observe another issue that seem related: if we run multiple
      // MSYS2-based applications in parallel (either from the same process
      // or from several processes), then they sometimes terminate abnormally
      // (but quietly, without printing any of the cygheap/fork diagnostics)
      // with status 0xC0000142 (STATUS_DLL_INIT_FAILED).
      //
      // Cygwin FAQ suggests the following potential solutions:
      //
      // 1. Restart the process hoping things will pan out next time around.
      //
      // 2. Eliminate/disable programs from BLODA (disabling Defender helps
      //    a lot but not entirely).
      //
      // 3. Apparently switching from 32 to 64-bit should help (less chance
      //    for address collisions).
      //
      // 4. Rebase all the Cygwin DLLs (this is a topic for another episode).
      //
      // To add to this list, we also have our own remedy (which is not
      // generally applicable):
      //
      // 5. Make sure processes that you start don't need to fork. A good
      //    example would be tar that runs gz/bzip2/xz. Instead, we start and
      //    pipe them ourselves.
      //
      // So what's coming next is a hack that implements remedy #1: after
      // starting the process we wait a bit and check if it has terminated
      // with STATUS_DLL_INIT_FAILED (the assumption here is that if this
      // happens, it happens quickly). We then retry starting the process for
      // some time.
      //
      // One way to improve this implementation is to only do it for MSYS2-
      // based programs. This way we can then wait longer and try harder. We
      // first used EnumProcessModules() to see if the process loaded the
      // msys-2.0.dll but that proved racy. Now we examine the PE executable's
      // import list.
      //

      // Notes:
      //
      // 1. The ImageLoad() API is not thread-safe so we do this while holding
      //    process_spawn_mutex.
      //
      // 2. We can only load an image of the same width as us (32/64-bit).
      //
      // 3. If something goes wrong (width mismatch, failed to load, etc),
      //    then we assume it is not MSYS (we used to do it the other way
      //    around for safety but that proved to be too slow).
      //
      auto detect_msys = [] (const char* p) -> bool
      {
        auto deleter = [] (LOADED_IMAGE* p)
        {
          if (p != nullptr)
            ImageUnload (p);
        };

        unique_ptr<LOADED_IMAGE, void (*)(LOADED_IMAGE*)> img (
          ImageLoad (p, nullptr), deleter);

        if (img == nullptr)
          return false;

        if (img->FileHeader->OptionalHeader.NumberOfRvaAndSizes < 2)
          return false;

        auto rva_to_ptr = [&img] (DWORD rva) -> const void*
        {
          const IMAGE_NT_HEADERS* nh (img->FileHeader);
          const BYTE* base (img->MappedAddress);

          auto rva_to_sh = [nh] (DWORD rva) -> const IMAGE_SECTION_HEADER*
          {
            const IMAGE_SECTION_HEADER* s (IMAGE_FIRST_SECTION (nh));

            for (WORD i (0); i < nh->FileHeader.NumberOfSections; ++i, ++s)
            {
              DWORD v (s->Misc.VirtualSize);
              if (v == 0)
                v = s->SizeOfRawData;

              // Is the RVA within this section?
              //
              if (rva >= s->VirtualAddress && rva < s->VirtualAddress + v)
                return s;
            }

            return nullptr;
          };

          if (const IMAGE_SECTION_HEADER* sh = rva_to_sh (rva))
          {
            ptrdiff_t o (sh->VirtualAddress - sh->PointerToRawData);
            return base - o + rva;
          }
          else
            return nullptr;
        };

        auto imp (
          static_cast<const IMAGE_IMPORT_DESCRIPTOR*> (
            rva_to_ptr (
              img->FileHeader->OptionalHeader.DataDirectory[1].VirtualAddress)));

        if (imp == nullptr)
          return false;

        for (; imp->Name != 0 || imp->TimeDateStamp != 0; ++imp)
        {
          if (auto name = static_cast<const char*> (rva_to_ptr (imp->Name)))
          {
            if (icasecmp (name, "msys-2.0.dll") == 0)
              return true;
          }
        }

        return false;
      };

      bool msys (false);
      if (!batch)
      {
        string p (pp.effect_string ());
        auto i (detect_msys_cache_.find (p));

        if (i == detect_msys_cache_.end ())
        {
          msys = detect_msys (p.c_str ());
          detect_msys_cache_.emplace (move (p), msys);
        }
        else
          msys = i->second;
      }

      using namespace chrono;

      // Retry for about 1 hour.
      //
      system_clock::duration timeout (1h);
      for (size_t i (0);; ++i)
      {
        if (!CreateProcess (
              batch ? batch->c_str () : pp.effect_string (),
              const_cast<char*> (cmd_line.c_str ()),
              0,    // Process security attributes.
              0,    // Primary thread security attributes.
              true, // Inherit handles.
              0,    // Creation flags.
              new_env.empty () ? nullptr : new_env.data (),
              cwd != nullptr && *cwd != '\0' ? cwd : nullptr,
              &si,
              &pi))
          fail ();

        auto_handle (pi.hThread).reset (); // Close.

        if (msys)
        {
          // Wait a bit and check if the process has terminated, or has written
          // some data to stdout or stderr. If it is still running then we
          // assume all is good. Otherwise, retry if this is the DLL
          // initialization error.
          //
          // Note that the process standard output streams probing is possible
          // only if they are connected to pipes created by us. We assume that
          // data presence at the reading end of any of these pipes means that
          // DLLs initialization successfully passed.
          //
          // If the process output stream is redirected to a pipe, check that
          // we have access to its reading end to probe it for data presence.
          //
          // @@ Unfortunately we can't distinguish a pipe that erroneously
          //    missing the reading end from the pipelined program output
          //    stream.
#if 0
          auto bad_pipe = [&get_osfhandle] (const pipe& p) -> bool
          {
            if (p.in != -1  ||  // Pipe provided by the user.
                p.out == -1 ||  // Pipe created by ctor.
                p.out == -2 ||  // Null device.
                fdterm (p.out)) // Terminal.
              return false;

            // No reading end, so make sure that the file descriptor is a pipe.
            //
            return GetFileType (get_osfhandle (p.out, false)) ==
                   FILE_TYPE_PIPE;
          };

          if (bad_pipe (pout) || bad_pipe (perr))
            assert (false);
#endif

          system_clock::time_point st (system_clock::now ());

          // Unlock the mutex to let other processes to be spawned while we are
          // waiting. We also need to revert handles to non-inheritable state
          // to prevent their leakage.
          //
          il.unlock ();
          l.unlock ();

          // Probe for data presence the reading end of the pipe that is
          // connected to the process standard output/error stream. Note that
          // the pipe can be created by us or provided by the user.
          //
          auto probe_fd = [&get_osfhandle] (int fd, int ufd) -> bool
          {
            // Can't be both created by us and provided by the user.
            //
            assert (fd == -1 || ufd == -1);

            if (fd == -1 && ufd == -1)
              return false;

            char c;
            DWORD n;
            HANDLE h (get_osfhandle (fd != -1 ? fd : ufd, false));
            return PeekNamedPipe (h, &c, 1, &n, nullptr, nullptr) && n == 1;
          };

          // Hidden by butl::duration that is introduced via fdstream.hxx.
          //
          using milli_duration = chrono::duration<DWORD, milli>;

          DWORD r;
          system_clock::duration twd (0);  // Total wait time.

          // We wait for non-whitelisted programs indefinitely, until it
          // terminates or prints to stdout/stderr. For whitelisted we wait
          // for half a second. Any MSYS program that reads from stdin prior
          // to writing to piped stdout/stderr must be whitelisted.
          //
          const char* wl[] = {"less.exe"};
          const char** wle = wl + sizeof (wl) / sizeof (wl[0]);

          bool w (find (wl, wle, pp.effect.leaf ().string ()) != wle);

          for (size_t n (0);; ++n) // Wait n times by 100ms.
          {
            milli_duration wd (100);

            r = WaitForSingleObject (pi.hProcess, wd.count ());
            twd += wd;

            if (r != WAIT_TIMEOUT ||
                probe_fd (in_ofd.in.get (), pout.in) ||
                probe_fd (in_efd.in.get (), perr.in) ||
                (w && n == 4))
              break;
          }

          if (r == WAIT_OBJECT_0                   &&
              GetExitCodeProcess (pi.hProcess, &r) &&
              r == STATUS_DLL_INIT_FAILED)
          {
            // Use exponential backoff with the up to a second delay. This
            // avoids unnecessary thrashing without allowing anyone else to
            // make progress.
            //
            milli_duration wd (static_cast<DWORD> (i < 32 ? i * i : 1000));
            Sleep (wd.count ());
            twd += wd;

            // Assume we have waited the full amount if the time adjustment is
            // detected.
            //
            system_clock::time_point now (system_clock::now ());
            system_clock::duration d (now > st ? now - st : twd);

            // If timeout is not fully exhausted, re-lock the mutex, revert
            // handles to inheritable state and re-spawn the process.
            //
            if (timeout > d)
            {
              timeout -= d;
              l.lock ();
              il.lock ();
              continue;
            }
          }
        }

        break;
      }
    } // Revert handles back to non-inheritable and release the lock.

    // 0 has a special meaning denoting a terminated process handle.
    //
    this->handle = pi.hProcess;
    assert (this->handle != 0 && this->handle != INVALID_HANDLE_VALUE);

    this->out_fd = move (out_fd.out);
    this->in_ofd = move (in_ofd.in);
    this->in_efd = move (in_efd.in);
  }

  bool process::
  wait (bool ie)
  {
    if (handle != 0)
    {
      out_fd.reset ();
      in_ofd.reset ();
      in_efd.reset ();

      DWORD es;
      DWORD e (NO_ERROR);
      if (WaitForSingleObject (handle, INFINITE) != WAIT_OBJECT_0 ||
          !GetExitCodeProcess (handle, &es))
        e = GetLastError ();

      auto_handle h (handle); // Auto-deleter.
      handle = 0; // We have tried.

      if (e == NO_ERROR)
      {
        exit = process_exit ();
        exit->status = es;
      }
      else
      {
        // If ignore errors then just leave exit nullopt, so it has "no exit
        // information available" semantics.
        //
        if (!ie)
          throw process_error (error_msg (e));
      }
    }

    return exit && exit->normal () && exit->code () == 0;
  }

  optional<bool> process::
  try_wait ()
  {
    return timed_wait (chrono::milliseconds (0));
  }

  template <>
  optional<bool> process::
  timed_wait (const chrono::milliseconds& t)
  {
    if (handle != 0)
    {
      DWORD r (WaitForSingleObject (handle, static_cast<DWORD> (t.count ())));
      if (r == WAIT_TIMEOUT)
        return nullopt;

      DWORD es;
      DWORD e (NO_ERROR);
      if (r != WAIT_OBJECT_0 || !GetExitCodeProcess (handle, &es))
        e = GetLastError ();

      auto_handle h (handle);
      handle = 0; // We have tried.

      if (e != NO_ERROR)
        throw process_error (error_msg (e));

      exit = process_exit ();
      exit->status = es;
    }

    return exit ? static_cast<bool> (*exit) : optional<bool> ();
  }

  void process::
  kill ()
  {
    // Note that TerminateProcess() requires an exit code the process will be
    // terminated with. We could probably craft a custom exit code that will
    // be treated by the normal() function as an abnormal termination.
    // However, let's keep it simple and reuse the existing (semantically
    // close) error code.
    //
    if (handle != 0 && !TerminateProcess (handle, DBG_TERMINATE_PROCESS))
    {
      DWORD e (GetLastError ());
      if (e != ERROR_ACCESS_DENIED)
        throw process_error (error_msg (e));

      // Handle the case when the process has already terminated or is still
      // exiting (potentially after being killed).
      //
      if (!try_wait ())
        throw process_error (error_msg (e), EPERM);
    }
  }

  void process::
  term ()
  {
    kill ();
  }

  process::id_type process::
  id () const
  {
    id_type r (GetProcessId (handle));

    if (r == 0)
      throw process_error (last_error_msg ());

    return r;
  }

  process::id_type process::
  current_id ()
  {
    return GetCurrentProcessId ();
  }

  process::handle_type process::
  current_handle ()
  {
    // Note that the returned handle is a pseudo handle (-1) that does not
    // need to be closed.
    //
    return GetCurrentProcess ();
  }

  // process_exit
  //
  process_exit::
  process_exit (code_type c)
      //
      // The NTSTATUS value returned by GetExitCodeProcess() has the following
      // layout of bits:
      //
      // [ 0, 16) - program exit code or exception code
      // [16, 29) - facility
      // [29, 30) - flag indicating if the status value is customer-defined
      // [30, 31] - severity (00 -success, 01 - informational, 10 - warning,
      //            11 - error)
      //
      : status (c)
  {
  }

  bool process_exit::
  normal () const
  {
    // We consider status values with severities other than 0 not being
    // returned by the process and so denoting the abnormal termination.
    //
    return ((status >> 30) & 0x3) == 0;
  }

  process_exit::code_type process_exit::
  code () const
  {
    assert (normal ());
    return status & 0xFFFF;
  }

  string process_exit::
  description () const
  {
    assert (!normal ());

    // Error codes (or, as MSDN calls them, exception codes) are defined in
    // ntstatus.h. It is possible to obtain message descriptions for them
    // using FormatMessage() with the FORMAT_MESSAGE_FROM_HMODULE flag and the
    // handle returned by LoadLibrary("NTDLL.DLL") call. However, the returned
    // messages are pretty much useless being format strings. For example for
    // STATUS_ACCESS_VIOLATION error code the message string is "The
    // instruction at 0x%p referenced memory at 0x%p. The memory could not be
    // %s.". Also under Wine (1.9.8) it is not possible to obtain such a
    // descriptions at all for some reason.
    //
    // Let's use a custom code-to-message mapping for the most common error
    // codes, and extend it as needed.
    //
    // Note that the error code most likely will be messed up if the abnormal
    // termination of a process is intercepted with the "searching for
    // available solution" message box or debugger invocation. Also note that
    // the same failure can result in different exit codes for a process being
    // run on Windows nativelly and under Wine. For example under Wine 1.9.8 a
    // process that fails due to the stack overflow exits normally with 0
    // status but prints the "err:seh:setup_exception stack overflow ..."
    // message to stderr.
    //
    switch (status)
    {
    case STATUS_ACCESS_VIOLATION:       return "access violation";
    case STATUS_DLL_INIT_FAILED:        return "DLL initialization failed";
    case STATUS_DLL_NOT_FOUND:          return "unable to find DLL";
    case STATUS_INTEGER_DIVIDE_BY_ZERO: return "integer divided by zero";

    // If a VC-compiled program exits with the STATUS_STACK_BUFFER_OVERRUN
    // (0xC0000409) status, it is not necessarily because of the stack buffer
    // overrun. This may also happen if the program called abort() due, for
    // example, to an unhandled exception or an assertion failure (see the
    // 'STATUS_STACK_BUFFER_OVERRUN doesn't mean that there was a stack buffer
    // overrun' Microsoft development blog article for details). That's why we
    // use the more general description for this error code.
    //
    // Note that MinGW GCC-compiled program exits normally with the status 3
    // if it calls abort() (conforms to MSDN). Under Wine (1.9.8) such a
    // behavior is common for both VC and MinGW GCC.
    //
    case STATUS_STACK_BUFFER_OVERRUN: return "aborted";
    case STATUS_STACK_OVERFLOW:       return "stack overflow";

    // Presumably the kill() function was called for the process.
    //
    case DBG_TERMINATE_PROCESS: return "killed";

    default:
      {
        string desc ("unknown error 0x");

        // Add error code hex representation (as it is defined in ntstatus.h).
        //
        // Strange enough, there is no easy way to convert a number into the
        // hex string representation (not using streams).
        //
        const char digits[] = "0123456789ABCDEF";
        bool skip (true); // Skip leading zeros.

        auto add = [&desc, &digits, &skip] (unsigned char d, bool force)
        {
          if (d != 0 || !skip || force)
          {
            desc += digits[d];
            skip = false;
          }
        };

        for (int i (sizeof (status) - 1); i >= 0 ; --i)
        {
          unsigned char c ((status >> (i * 8)) & 0xFF);
          add ((c >> 4) & 0xF, false); // Convert the high 4 bits to a digit.
          add (c & 0xF, i == 0);       // Convert the low 4 bits to a digit.
        }

        return desc;
      }
    }
  }

#endif // _WIN32
}
