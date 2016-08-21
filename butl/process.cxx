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

#  include <io.h>        // _open_osfhandle(), _get_osfhandle(), _close()
#  include <fcntl.h>     // _O_TEXT
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

#include <cassert>
#include <cstddef> // size_t
#include <cstring> // strlen(), strchr()

#include <butl/utility>  // casecmp()
#include <butl/fdstream> // fdnull(), fdclose()

using namespace std;

#ifdef _WIN32
using namespace butl::win32;
#endif

namespace butl
{
  class auto_fd
  {
  public:
    explicit
    auto_fd (int fd = -1) noexcept: fd_ (fd) {}

    auto_fd (const auto_fd&) = delete;
    auto_fd& operator= (const auto_fd&) = delete;

    ~auto_fd () noexcept {reset ();}

    int
    get () const noexcept {return fd_;}

    int
    release () noexcept
    {
      int r (fd_);
      fd_ = -1;
      return r;
    }

    void
    reset (int fd = -1) noexcept
    {
      if (fd_ != -1)
      {
        bool r (fdclose (fd_));

        // The valid file descriptor that has no IO operations being
        // performed on it should close successfully, unless something is
        // severely damaged.
        //
        assert (r);
      }

      fd_ = fd;
    }

  private:
    int fd_;
  };

#ifndef _WIN32

  process_path process::
  path_search (const char*& args0)
  {
    // Note that there is a similar version for Win32.

    typedef path::traits traits;

    const char* f (args0);
    size_t fn (strlen (f));

    path rp, ep; // recall & effective
    auto search = [&ep, f, fn] (const char* d, size_t dn) -> bool
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

      // Check that the file exists and has at least one executable bit set.
      // This way we get a bit closer to the "continue search on EACCES"
      // semantics (see below).
      //
      struct stat si;
      return (stat (ep.string ().c_str (), &si) == 0 &&
              S_ISREG (si.st_mode) &&
              (si.st_mode & (S_IEXEC | S_IXGRP | S_IXOTH)) != 0);
    };

    for (;;) // The "goto end" loop.
    {
      // If there is a directory component in the file, then search does not
      // apply.
      //
      if (traits::find_separator (f, fn) != nullptr)
        break;

      // The search order is documented in exec(3). Some of the differences
      // compared to exec*p() functions:
      //
      // 1. If there no PATH, we don't default to current directory/_CS_PATH.
      // 2. We do not continue searching on EACCES from execve().
      // 3. We do not execute via default shell on ENOEXEC from execve().
      //
      {
        const char* b (getenv ("PATH"));

        for (const char* e; b != nullptr; b = (e != nullptr ? e + 1 : e))
        {
          e = strchr (b, traits::path_separator);

          // Empty path (i.e., a double colon or a colon at the beginning or
          // end of PATH) means search in the current dirrectory.
          //
          if (search (b, e != nullptr ? e - b : strlen (b)))
            break;
        }

        if (b != nullptr)
          break;
      }

      // Did not find anything.
      //
      throw process_error (ENOENT, false);
    }

    // Found the file and the result is in rp and ep, both of which can be
    // empty.
    //
    process_path r (f,
                    rp.empty () ? nullptr : &(args0 = rp.string ().c_str ()));

    r.recall = move (rp);
    r.effect = move (ep);

    return r;
  }

  process::
  process (const char* cwd,
           const process_path& pp, const char* args[],
           int in, int out, int err)
  {
    using pipe = auto_fd[2];

    pipe out_fd;
    pipe in_ofd;
    pipe in_efd;

    auto fail = [](bool child) {throw process_error (errno, child);};

    auto create_pipe = [&fail](pipe& p)
    {
      int pd[2];
      if (::pipe (pd) == -1)
        fail (false);

      p[0].reset (pd[0]);
      p[1].reset (pd[1]);
    };

    auto create_null = [&fail](auto_fd& n)
    {
      int fd (fdnull ());
      if (fd == -1)
        fail (false);

      n.reset (fd);
    };

    // If we are asked to open null (-2) then open "half-pipe".
    //
    if (in == -1)
      create_pipe (out_fd);
    else if (in == -2)
      create_null (out_fd[0]);

    if (out == -1)
      create_pipe (in_ofd);
    else if (out == -2)
      create_null (in_ofd[1]);

    if (err == -1)
      create_pipe (in_efd);
    else if (err == -2)
      create_null (in_efd[1]);

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
      auto duplicate = [&fail](int sd, int fd, pipe& pd)
      {
        if (fd == -1 || fd == -2)
          fd = pd[sd == STDIN_FILENO ? 0 : 1].get ();

        assert (fd > -1);
        if (dup2 (fd, sd) == -1)
          fail (true);

        pd[0].reset (); // Close.
        pd[1].reset (); // Close.
      };

      if (in != STDIN_FILENO)
        duplicate (STDIN_FILENO, in, out_fd);

      if (out != STDOUT_FILENO)
        duplicate (STDOUT_FILENO, out, in_ofd);

      if (err != STDERR_FILENO)
        duplicate (STDERR_FILENO, err, in_efd);

      // Change current working directory if requested.
      //
      if (cwd != nullptr && *cwd != '\0' && chdir (cwd) != 0)
        fail (true);

      const char* file (pp.effect.empty ()
                        ? args[0]
                        : pp.effect.string ().c_str ());

      if (execv (file, const_cast<char**> (&args[0])) == -1)
        fail (true);
    }

    assert (handle != 0); // Shouldn't get here unless in the parent process.

    this->out_fd = out_fd[1].release ();
    this->in_ofd = in_ofd[0].release ();
    this->in_efd = in_efd[0].release ();
  }

  process::
  process (const char* cwd,
           const process_path& pp, const char* args[],
           process& in, int out, int err)
      : process (cwd, pp, args, in.in_ofd, out, err)
  {
    assert (in.in_ofd != -1); // Should be a pipe.
    close (in.in_ofd); // Close it on our side.
  }

  bool process::
  wait (bool ie)
  {
    if (handle != 0)
    {
      int r (waitpid (handle, &status, 0));
      handle = 0; // We have tried.

      if (r == -1)
      {
        if (!ie)
          throw process_error (errno, false);
        else
          // Fold into status, so this and subsequent wait() calls return
          // false. There is no portable way to update the status bits
          // representing a process exit code specifically. So we set all bits
          // to 1 and recon on getting non-zero exit status wherever the exact
          // bits are.
          //
          status = ~0;
      }
    }

    return WIFEXITED (status) && WEXITSTATUS (status) == 0;
  }

  bool process::
  try_wait (bool& s)
  {
    if (handle != 0)
    {
      int r (waitpid (handle, &status, WNOHANG));

      if (r == 0) // Not exited yet.
        return false;

      handle = 0; // We have tried.

      if (r == -1)
        throw process_error (errno, false);
    }

    s =  WIFEXITED (status) && WEXITSTATUS (status) == 0;
    return true;
  }

  process::id_type process::
  current_id ()
  {
    return getpid ();
  }

#else // _WIN32

  process_path process::
  path_search (const char*& args0)
  {
    // Note that there is a similar version for Win32.

    typedef path::traits traits;

    const char* f (args0);
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

    path rp, ep; // recall & effective
    auto search = [&ep, f, fn, ext] (const char* d, size_t dn) -> bool
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

      // Add the .exe extension if necessary.
      //
      if (ext)
        ep += ".exe";

      // Only check that the file exists since the executable mode is set
      // according to the file extension.
      //
      struct _stat si;
      return _stat (ep.string ().c_str (), &si) == 0 && S_ISREG (si.st_mode);
    };

    for (;;) // The "goto end" loop.
    {
      // If there is a directory component in the file, then search does not
      // apply. But we may still need to append the extension.
      //
      if (traits::find_separator (f, fn) != nullptr)
      {
        if (ext)
        {
          ep = path (f, fn);
          ep += ".exe";
        }

        break;
      }

      // The search order is documented in CreateProcess(). First we look in
      // the directory of the parent executable.
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
          // ideally, we would want to use parent's argv[0] dir (if any) to
          // form the recall path. In particular, if the parent has no
          // directory, then it means it was found via the standard search
          // (e.g., PATH) and then so should the child.
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
          }

          break;
        }
      }

      // Next look in the current working directory. Crazy, I know.
      //
      // The recall path is the same as initial, though it might not be a bad
      // idea to prepend .\ for clarity.
      //
      if (search ("", 0))
        break;

      // Finally, search in PATH. Recall is unchanged.
      //
      {
        const char* b (getenv ("PATH"));

        for (const char* e; b != nullptr; b = (e != nullptr ? e + 1 : e))
        {
          e = strchr (b, traits::path_separator);

          // Empty path (i.e., a double colon or a colon at the beginning or
          // end of PATH) means search in the current dirrectory.
          //
          if (search (b, e != nullptr ? e - b : strlen (b)))
            break;
        }

        if (b != nullptr)
          break;
      }

      // Did not find anything.
      //
      throw process_error (ENOENT);
    }

    // Found the file and the result is in rp and ep, both of which can be
    // empty.
    //
    process_path r (f,
                    rp.empty () ? nullptr : &(args0 = rp.string ().c_str ()));

    r.recall = move (rp);
    r.effect = move (ep);

    return r;
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
    using pipe = auto_handle[2];

    pipe out_h;
    pipe in_oh;
    pipe in_eh;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof (SECURITY_ATTRIBUTES);
    sa.bInheritHandle = true;
    sa.lpSecurityDescriptor = 0;

    auto fail = [](const char* m = nullptr)
    {
      throw process_error (m == nullptr ? last_error_msg () : m);
    };

    // Create a pipe and clear the inherit flag on the parent side.
    //
    auto create_pipe = [&sa, &fail](pipe& p, int parent)
    {
      HANDLE ph[2];
      if (!CreatePipe (&ph[0], &ph[1], &sa, 0))
        fail ();

      p[0].reset (ph[0]);
      p[1].reset (ph[1]);

      if (!SetHandleInformation (p[parent].get (), HANDLE_FLAG_INHERIT, 0))
        fail ();
    };

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

    auto create_null = [&get_osfhandle, &fail](auto_handle& n)
    {
      // Note that we are using a faster, temporary file-based emulation of
      // NUL since we have no way of making sure the child buffers things
      // properly (and by default they seem no to).
      //
      auto_fd fd (fdnull (true));
      if (fd.get () == -1)
        fail ();

      n.reset (get_osfhandle (fd.get ()));
      fd.release (); // Not to close the handle twice.
    };

    if (in == -1)
      create_pipe (out_h, 1);
    else if (in == -2)
      create_null (out_h[0]);

    if (out == -1)
      create_pipe (in_oh, 0);
    else if (out == -2)
      create_null (in_oh[1]);

    if (err == -1)
      create_pipe (in_eh, 0);
    else if (err == -2)
      create_null (in_eh[1]);

    // Create the process.
    //
    const char* file (pp.effect.empty ()
                      ? args[0]
                      : pp.effect.string ().c_str ());

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
      bool quote (a.find (' ') != string::npos);

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
      ? out_h[0].get ()
      : in == STDIN_FILENO
        ? GetStdHandle (STD_INPUT_HANDLE)
        : get_osfhandle (in);

    si.hStdOutput = out == -1 || out == -2
      ? in_oh[1].get ()
      : out == STDOUT_FILENO
        ? GetStdHandle (STD_OUTPUT_HANDLE)
        : get_osfhandle (out);

    si.hStdError = err == -1 || err == -2
      ? in_eh[1].get ()
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
          file,
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

    // Convert file handles to file descriptors. Note that the handle is
    // closed when _close() is called for the returned file descriptor.
    //
    auto open_osfhandle = [&fail](auto_handle& h) -> int
    {
      int fd (
        _open_osfhandle (reinterpret_cast<intptr_t> (h.get ()), _O_TEXT));

      if (fd == -1)
        fail ("unable to convert file handle to file descriptor");

      h.release ();
      return fd;
    };

    auto_fd out_fd (in == -1 ? open_osfhandle (out_h[1]) : -1);
    auto_fd in_ofd (out == -1 ? open_osfhandle (in_oh[0]) : -1);
    auto_fd in_efd (err == -1 ? open_osfhandle (in_eh[0]) : -1);

    this->out_fd = out_fd.release ();
    this->in_ofd = in_ofd.release ();
    this->in_efd = in_efd.release ();

    this->handle = process.release ();

    // 0 has a special meaning denoting a terminated process handle.
    //
    assert (this->handle != 0 && this->handle != INVALID_HANDLE_VALUE);
  }

  process::
  process (const char* cwd,
           const process_path& pp, const char* args[],
           process& in, int out, int err)
      : process (cwd, pp, args, in.in_ofd, out, err)
  {
    assert (in.in_ofd != -1); // Should be a pipe.
    _close (in.in_ofd); // Close it on our side.
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
        if (!ie)
          throw process_error (error_msg (e));
        else
          status = 1; // Fold into status.
      }
    }

    return status == 0;
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

    s = status == 0;
    return true;
  }

  process::id_type process::
  current_id ()
  {
    return GetCurrentProcessId ();
  }

#endif // _WIN32
}
