// file      : butl/process.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
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
#  include <stdlib.h>    // _MAX_PATH, getenv()
#  include <sys/types.h> // stat
#  include <sys/stat.h>  // stat(), S_IS*

#  ifdef _MSC_VER // Unlikely to be fixed in newer versions.
#    define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)

#    define STDIN_FILENO  0
#    define STDOUT_FILENO 1
#    define STDERR_FILENO 2
#  endif // _MSC_VER

#  include <memory>    // unique_ptr
#  include <cstdlib>   // __argv[]

#  include <butl/win32-utility>
#endif

#include <errno.h>

#include <ios>     // ios_base::failure
#include <cassert>
#include <cstddef> // size_t
#include <cstring> // strlen(), strchr()
#include <utility> // move()
#include <ostream>

#include <butl/utility>  // casecmp()
#include <butl/fdstream> // fdnull()

using namespace std;

#ifdef _WIN32
using namespace butl::win32;
#endif

namespace butl
{
  static process_path
  path_search (const char*, const dir_path&);

  process_path process::
  path_search (const char* f, bool init, const dir_path& fb)
  {
    process_path r (try_path_search (f, init, fb));

    if (r.empty ())
      throw process_error (ENOENT, false);

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
        const string& d (traits::current ());

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

    auto fail = [](bool child) {throw process_error (errno, child);};

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
        throw process_error (EMFILE, false);
      }
    };

    auto open_null = [&fail] () -> auto_fd
    {
      auto_fd fd (fdnull ());
      if (fd.get () == -1)
        fail (false);

      return fd;
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

    handle = fork ();

    if (handle == -1)
      fail (false);

    if (handle == 0)
    {
      // Child.
      //
      // Duplicate the user-supplied (fd > -1) or the created pipe descriptor
      // to the standard stream descriptor (read end for STDIN_FILENO, write
      // end otherwise). Close the the pipe afterwards.
      //
      auto duplicate = [&fail](int sd, int fd, fdpipe& pd)
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
      status_type es;
      int r (waitpid (handle, &es, 0));
      handle = 0; // We have tried.

      if (r == -1)
      {
        // If ignore errors then just leave status nullopt, so it has the same
        // semantics as for abnormally terminated process.
        //
        if (!ie)
          throw process_error (errno, false);
      }
      else if (WIFEXITED (es))
        status = WEXITSTATUS (es);
    }

    return status && *status == 0;
  }

  bool process::
  try_wait (bool& s)
  {
    if (handle != 0)
    {
      status_type es;
      int r (waitpid (handle, &es, WNOHANG));

      if (r == 0) // Not exited yet.
        return false;

      handle = 0; // We have tried.

      if (r == -1)
        throw process_error (errno, false);

      if (WIFEXITED (es))
        status = WEXITSTATUS (es);
    }

    s = status && *status == 0;
    return true;
  }

  process::id_type process::
  current_id ()
  {
    return getpid ();
  }

#else // _WIN32

  static process_path
  path_search (const char* f, const dir_path& fb)
  {
    // Note that there is a similar version for Win32.

    typedef path::traits traits;

    size_t fn (strlen (f));

    // Unless there is already the .exe extension, then we will need to add
    // it. Note that running .bat files requires starting cmd.exe and passing
    // the batch file as an argument (see CreateProcess() for deails). So
    // if/when we decide to support those, it will have to be handled
    // differently.
    //
    bool ext;
    {
      const char* e (traits::find_extension (f, fn));
      ext = (e == nullptr || casecmp (e, ".exe") != 0);
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

    auto search = [&ep, f, fn, ext, &exists] (const char* d,
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

      // Add the .exe extension if necessary.
      //
      if (ext)
        ep += ".exe";

      return exists (ep.string ().c_str ());
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
        if (ext)
        {
          ep = path (f, fn);
          ep += ".exe";
        }

        if (exists (r.effect_string ()))
          return r;
      }
      else
      {
        const string& d (traits::current ());

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
      const string& d (traits::current ());

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

  process::
  process (const char* cwd,
           const process_path& pp, const char* args[],
           int in, int out, int err)
  {
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

    auto open_null = [&fail] () -> auto_fd
    {
      // Note that we are using a faster, temporary file-based emulation of
      // NUL since we have no way of making sure the child buffers things
      // properly (and by default they seem no to).
      //
      auto_fd fd (fdnull (true));
      if (fd.get () == -1)
        fail ();

      return fd;
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

    // Resolve file descriptor to HANDLE and make sure it is inherited. Note
    // that the handle is closed either when CloseHandle() is called for it or
    // when _close() is called for the associated file descriptor. Make sure
    // that either the original file descriptor or the resulted HANDLE is
    // closed but not both of them.
    //
    auto get_osfhandle = [&fail](int fd) -> HANDLE
    {
      HANDLE h (reinterpret_cast<HANDLE> (_get_osfhandle (fd)));
      if (h == INVALID_HANDLE_VALUE)
        fail ("unable to obtain file handle");

      // SetHandleInformation() fails for standard handles. We assume they are
      // inherited by default.
      //
      if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
      {
        if (!SetHandleInformation (
              h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
          fail ();
      }

      return h;
    };

    // Create the process.
    //

    // Serialize the arguments to string.
    //
    string cmd_line;

    for (const char* const* p (args); *p != 0; ++p)
    {
      if (p != args)
        cmd_line += ' ';

      // On Windows we need to protect values with spaces using quotes. Since
      // there could be actual quotes in the value, we need to escape them.
      //
      string a (*p);
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
    }

    // Prepare other process information.
    //
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    memset (&si, 0, sizeof (STARTUPINFO));
    memset (&pi, 0, sizeof (PROCESS_INFORMATION));

    si.cb = sizeof (STARTUPINFO);
    si.dwFlags |= STARTF_USESTDHANDLES;

    si.hStdInput = in == -1 || in == -2
      ? get_osfhandle (out_fd.in.get ())
      : in == STDIN_FILENO
        ? GetStdHandle (STD_INPUT_HANDLE)
        : get_osfhandle (in);

    si.hStdOutput = out == -1 || out == -2
      ? get_osfhandle (in_ofd.out.get ())
      : out == STDOUT_FILENO
        ? GetStdHandle (STD_OUTPUT_HANDLE)
        : get_osfhandle (out);

    si.hStdError = err == -1 || err == -2
      ? get_osfhandle (in_efd.out.get ())
      : err == STDERR_FILENO
        ? GetStdHandle (STD_ERROR_HANDLE)
        : get_osfhandle (err);

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
          pp.effect_string (),
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

    auto_handle (pi.hThread).reset (); // Close.
    auto_handle process (pi.hProcess);

    this->out_fd = move (out_fd.out);
    this->in_ofd = move (in_ofd.in);
    this->in_efd = move (in_efd.in);

    this->handle = process.release ();

    // 0 has a special meaning denoting a terminated process handle.
    //
    assert (this->handle != 0 && this->handle != INVALID_HANDLE_VALUE);
  }

  bool process::
  wait (bool ie)
  {
    if (handle != 0)
    {
      DWORD s;
      DWORD e (NO_ERROR);
      if (WaitForSingleObject (handle, INFINITE) != WAIT_OBJECT_0 ||
          !GetExitCodeProcess (handle, &s))
        e = GetLastError ();

      auto_handle h (handle); // Auto-deleter.
      handle = 0; // We have tried.

      if (e == NO_ERROR)
        status = s;
      else
      {
        // If ignore errors then just leave status nullopt, so it has the same
        // semantics as for abnormally terminated process.
        //
        if (!ie)
          throw process_error (error_msg (e));
      }
    }

    return status && *status == 0;
  }

  bool process::
  try_wait (bool& s)
  {
    if (handle != 0)
    {
      DWORD r (WaitForSingleObject (handle, 0));
      if (r == WAIT_TIMEOUT)
        return false;

      DWORD s;
      DWORD e (NO_ERROR);
      if (r != WAIT_OBJECT_0 || !GetExitCodeProcess (handle, &s))
        e = GetLastError ();

      auto_handle h (handle);
      handle = 0; // We have tried.

      if (e != NO_ERROR)
        throw process_error (error_msg (e));

      status = s;
    }

    s = status && *status == 0;
    return true;
  }

  process::id_type process::
  current_id ()
  {
    return GetCurrentProcessId ();
  }

#endif // _WIN32
}
