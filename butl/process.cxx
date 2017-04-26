// file      : butl/process.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/process>

#ifndef _WIN32
#  include <unistd.h>    // execvp, fork, dup2, pipe, chdir, *_FILENO, getpid
#  include <sys/wait.h>  // waitpid
#  include <sys/types.h> // _stat
#  include <sys/stat.h>  // _stat(), S_IS*
#else
#  include <butl/win32-utility>

#  include <io.h>        // _get_osfhandle(), _close()
#  include <stdlib.h>    // _MAX_PATH
#  include <sys/types.h> // stat
#  include <sys/stat.h>  // stat(), S_IS*

#  ifdef _MSC_VER // Unlikely to be fixed in newer versions.
#    define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)

#    define STDIN_FILENO  0
#    define STDOUT_FILENO 1
#    define STDERR_FILENO 2
#  endif // _MSC_VER

#  include <cstdlib> // getenv(), __argv[]

#  include <butl/small-vector>
#endif

#include <errno.h>

#include <ios>      // ios_base::failure
#include <cassert>
#include <cstddef>  // size_t
#include <cstring>  // strlen(), strchr()
#include <utility>  // move()
#include <ostream>

#include <butl/utility>         // casecmp()
#include <butl/fdstream>        // fdnull()
#include <butl/process-details>

using namespace std;

#ifdef _WIN32
using namespace butl::win32;
#endif

namespace butl
{
  shared_mutex process_spawn_mutex;

  // process
  //
  static process_path
  path_search (const char*, const dir_path&);

  process_path process::
  path_search (const char* f, bool init, const dir_path& fb)
  {
    process_path r (try_path_search (f, init, fb));

    if (r.empty ())
      throw process_error (ENOENT);

    return r;
  }

  process_path process::
  try_path_search (const char* f, bool init, const dir_path& fb)
  {
    process_path r (butl::path_search (f, fb));

    if (!init && !r.empty ())
    {
      path& rp (r.recall);
      r.initial = (rp.empty () ? (rp = path (f)) : rp).string ().c_str ();
    }

    return r;
  }

  void process::
  print (ostream& o, const char* const args[], size_t n)
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

  process::
  process (const char* cwd,
           const process_path& pp, const char* args[],
           process& in, int out, int err)
      : process (cwd, pp, args, in.in_ofd.get (), out, err)
  {
    assert (in.in_ofd.get () != -1); // Should be a pipe.
    in.in_ofd.reset (); // Close it on our side.
  }

#ifndef _WIN32

  static process_path
  path_search (const char* f, const dir_path& fb)
  {
    // Note that there is a similar version for Win32.

    typedef path::traits traits;

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
      ep = path (move (s)); // Move back into result.

      if (norm)
        ep.normalize ();

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
    // 2. We do not continue searching on EACCES from execve().
    // 3. We do not execute via default shell on ENOEXEC from execve().
    //
    for (const char* b (getenv ("PATH")), *e;
         b != nullptr;
         b = (e != nullptr ? e + 1 : e))
    {
      e = strchr (b, traits::path_separator);

      // Empty path (i.e., a double colon or a colon at the beginning or end
      // of PATH) means search in the current dirrectory. Silently skip
      // invalid paths.
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
  process (const char* cwd,
           const process_path& pp, const char* args[],
           int in, int out, int err)
  {
    fdpipe out_fd;
    fdpipe in_ofd;
    fdpipe in_efd;

    auto fail = [] (bool child)
    {
      if (child)
        throw process_child_error (errno);
      else
        throw process_error (errno);
    };

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
        return fdnull ();
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

    {
      ulock l (process_spawn_mutex); // Will not be released in child.
      handle = fork ();

      if (handle == -1)
        fail (false);

      if (handle == 0)
      {
        // Child.
        //
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
            fail (true);

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

          if (out != STDOUT_FILENO)
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
          fail (true);

        if (execv (pp.effect_string (), const_cast<char**> (&args[0])) == -1)
          fail (true);
      }
    } // Release the lock in parent.

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

  bool process::
  try_wait ()
  {
    if (handle != 0)
    {
      int es;
      int r (waitpid (handle, &es, WNOHANG));

      if (r == 0) // Not exited yet.
        return false;

      handle = 0; // We have tried.

      if (r == -1)
        throw process_error (errno);

      exit = process_exit (es, process_exit::as_status);
    }

    return true;
  }

  process::id_type process::
  current_id ()
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

    static_assert (WIFEXITED    (status_code) &&
                   WEXITSTATUS  (status_code) == 0xFF &&
                   !WIFSIGNALED (status_code),
                   "unexpected process exit status bits layout");
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
    default:        return "unknown signal " + to_string (signal ());
    }
  }

#else // _WIN32

  static process_path
  path_search (const char* f, const dir_path& fb)
  {
    // Note that there is a similar version for Win32.

    typedef path::traits traits;

    size_t fn (strlen (f));

    // Unless there is already the .exe/.bat extension, then we will need to
    // add it.
    //
    bool ext;
    {
      const char* e (traits::find_extension (f, fn));
      ext = (e == nullptr ||
             (casecmp (e, ".exe") != 0 &&
              casecmp (e, ".bat") != 0 &&
              casecmp (e, ".cmd") != 0));
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
        size_t n (strlen (d));
        if (const char* p = traits::rfind_separator (d, n))
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
    {
      const string& d (traits::current_directory ());

      if (search (d.c_str (), d.size ()))
        return r;
    }

    // Now search in PATH. Recall is unchanged.
    //
    for (const char* b (getenv ("PATH")), *e;
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

  class auto_handle
  {
  public:
    explicit
    auto_handle (HANDLE h = INVALID_HANDLE_VALUE) noexcept: handle_ (h) {}

    auto_handle (const auto_handle&) = delete;
    auto_handle& operator= (const auto_handle&) = delete;

    ~auto_handle () noexcept {reset ();}

    HANDLE
    get () const noexcept {return handle_;}

    HANDLE
    release () noexcept
    {
      HANDLE r (handle_);
      handle_ = INVALID_HANDLE_VALUE;
      return r;
    }

    void
    reset (HANDLE h = INVALID_HANDLE_VALUE) noexcept
    {
      if (handle_ != INVALID_HANDLE_VALUE)
      {
        bool r (CloseHandle (handle_));

        // The valid process, thread or file handle that has no IO operations
        // being performed on it should close successfully, unless something
        // is severely damaged.
        //
        assert (r);
      }

      handle_ = h;
    }

  private:
    HANDLE handle_;
  };

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
  class inheritability_guard
  {
  public:
    // Require the proof that the mutex is pre-acquired for exclusive access.
    //
    inheritability_guard (const ulock&) {}

    ~inheritability_guard ()
    {
      for (auto h: handles_)
        inheritable (h, false); // Can't throw.
    }

    void
    inheritable (HANDLE h)
    {
      inheritable (h, true);  // Can throw.
      handles_.push_back (h);
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

  private:
    small_vector<HANDLE, 3> handles_;
  };

  process::
  process (const char* cwd,
           const process_path& pp, const char* args[],
           int in, int out, int err)
  {
    // Figure out if this is a batch file since running them requires starting
    // cmd.exe and passing the batch file as an argument (see CreateProcess()
    // for deails).
    //
    const char* batch (nullptr);
    {
      const char* p (pp.effect_string ());
      const char* e (path::traits::find_extension (p, strlen (p)));
      if (e != nullptr && (casecmp (e, ".bat") == 0 ||
                           casecmp (e, ".cmd") == 0))
      {
        batch = getenv ("COMSPEC");

        if (batch == nullptr)
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

    auto fail = [](const char* m = nullptr)
    {
      throw process_error (m == nullptr ? last_error_msg () : m);
    };

    auto open_null = [] () -> auto_fd
    {
      // Note that we are using a faster, temporary file-based emulation of
      // NUL since we have no way of making sure the child buffers things
      // properly (and by default they seem no to).
      //
      try
      {
        return fdnull (true);
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
      auto append = [&cmd_line] (const string& a)
      {
        if (!cmd_line.empty ())
          cmd_line += ' ';

        // On Windows we need to protect values with spaces using quotes.
        // Since there could be actual quotes in the value, we need to escape
        // them.
        //
        bool quote (a.empty () || a.find (' ') != string::npos);

        if (quote)
          cmd_line += '"';

        for (size_t i (0); i < a.size (); ++i)
        {
          if (a[i] == '"')
            cmd_line += "\\\"";
          else
            cmd_line += a[i];
        }

        if (quote)
          cmd_line += '"';
      };

      if (batch != nullptr)
      {
        append (batch);
        append ("/c");
        append (pp.effect_string ());
      }

      for (const char* const* p (args + (batch != nullptr ? 1 : 0));
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
      inheritability_guard ig (l);

      // Resolve file descriptor to HANDLE and make sure it is inherited. Note
      // that the handle is closed either when CloseHandle() is called for it
      // or when _close() is called for the associated file descriptor. Make
      // sure that either the original file descriptor or the resulting HANDLE
      // is closed but not both of them.
      //
      auto get_osfhandle = [&fail, &ig] (int fd) -> HANDLE
      {
        HANDLE h (reinterpret_cast<HANDLE> (_get_osfhandle (fd)));
        if (h == INVALID_HANDLE_VALUE)
          fail ("unable to obtain file handle");

        // Make the handle inheritable by the child unless it is already
        // inheritable.
        //
        DWORD f;
        if (!GetHandleInformation (h, &f))
          fail ();

        // Note that the flag check is essential as SetHandleInformation()
        // fails for standard handles and their duplicates.
        //
        if ((f & HANDLE_FLAG_INHERIT) == 0)
          ig.inheritable (h);

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

      if (!CreateProcess (
            batch != nullptr ? batch : pp.effect_string (),
            const_cast<char*> (cmd_line.c_str ()),
            0,    // Process security attributes.
            0,    // Primary thread security attributes.
            true, // Inherit handles.
            0,    // Creation flags.
            0,    // Use our environment.
            cwd != nullptr && *cwd != '\0' ? cwd : nullptr,
            &si,
            &pi))
        fail ();
    } // Revert handled back to non-inheritable and release the lock.

    auto_handle (pi.hThread).reset (); // Close.

    this->out_fd = move (out_fd.out);
    this->in_ofd = move (in_ofd.in);
    this->in_efd = move (in_efd.in);

    this->handle = pi.hProcess;

    // 0 has a special meaning denoting a terminated process handle.
    //
    assert (this->handle != 0 && this->handle != INVALID_HANDLE_VALUE);
  }

  bool process::
  wait (bool ie)
  {
    if (handle != 0)
    {
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

  bool process::
  try_wait ()
  {
    if (handle != 0)
    {
      DWORD r (WaitForSingleObject (handle, 0));
      if (r == WAIT_TIMEOUT)
        return false;

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

    return true;
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
      // [30, 31) - severity (00 -success, 01 - informational, 10 - warning,
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
    case STATUS_INTEGER_DIVIDE_BY_ZERO: return "integer divided by zero";

    // VC-compiled program that calls abort() terminates with this error code
    // (0xC0000409). That differs from MinGW GCC-compiled one, that exits
    // normally with status 3 (conforms to MSDN). Under Wine (1.9.8) such a
    // program exits with status 3 for both VC and MinGW GCC. Sounds weird.
    //
    case STATUS_STACK_BUFFER_OVERRUN: return "stack buffer overrun";
    case STATUS_STACK_OVERFLOW:       return "stack overflow";

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
