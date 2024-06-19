// file      : libbutl/pager.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/pager.hxx>

#include <errno.h> // E*

#ifndef _WIN32
#  include <unistd.h>    // STDOUT_FILENO
#  include <sys/ioctl.h> // ioctl()
#else
#  include <libbutl/win32-utility.hxx>
#endif

#include <string>
#include <vector>
#include <cstddef>      // size_t
#include <cstring>      // strchr()
#include <utility>      // move()
#include <system_error> // system_error, generic_category()

#ifndef _WIN32
#  include <chrono>
#  include <thread> // this_thread::sleep_for()
#endif

#include <libbutl/utility.hxx>
#include <libbutl/optional.hxx>
#include <libbutl/fdstream.hxx>

using namespace std;

namespace butl
{
  pager::
  pager (const string& name,
         bool verbose,
         const string* pager,
         const vector<string>* pager_options)
      // Create successfully exited process. Will "wait" for it if fallback to
      // non-interactive execution path.
      //
      : p_ (process_exit (0))
  {
    // If we are using the default pager, try to get the terminal width
    // so that we can center the output.
    //
    if (pager == nullptr)
    {
      size_t col (0);

#ifndef _WIN32
#  ifdef TIOCGWINSZ
      struct winsize w;
      if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &w) == 0)
        col = static_cast<size_t> (w.ws_col);
#  endif
#else
      // This works properly on PowerShell, while for the regular console
      // (cmd.exe) it always returns 80 columns.
      //
      CONSOLE_SCREEN_BUFFER_INFO w;
      if (GetConsoleScreenBufferInfo (GetStdHandle (STD_OUTPUT_HANDLE), &w))
        col = static_cast<size_t> (w.srWindow.Right - w.srWindow.Left + 1);
#endif
      if (col > 80)
        indent_.assign ((col - 80) / 2, ' ');
    }

    vector<const char*> args;
    string prompt;

    if (pager != nullptr)
    {
      if (pager->empty ())
        return; // No pager should be used.

      args.push_back (pager->c_str ());
    }
    else
    {
      // By default try less (again, no pun intended).
      //
      prompt = "-Ps" + name + " (press q to quit, h for help)";

      args.push_back ("less");
      args.push_back ("-R");    // Handle ANSI color.

      args.push_back (prompt.c_str ());
    }

    // Add extra pager options.
    //
    if (pager_options != nullptr)
      for (const string& o: *pager_options)
        args.push_back (o.c_str ());

    args.push_back (nullptr);

    // On Windows if we are using default less, then set the TERM environment
    // variable to cygwin. Failed that, some environments like git-bash may
    // set it to some strange values (like xterm-256color) which confuses
    // less.
    //
    const char* env[] = {nullptr, nullptr};

#ifdef _WIN32
    if (pager == nullptr)
      env[0] = "TERM=cygwin";
#endif

    if (verbose)
    {
      for (const char* const* p (args.data ()); *p != nullptr; ++p)
      {
        if (p != args.data ())
          cerr << ' ';

        // Quote if empty or contains spaces.
        //
        bool q (**p == '\0' || strchr (*p, ' ') != nullptr);

        if (q)
          cerr << '"';

        cerr << *p;

        if (q)
          cerr << '"';
      }
      cerr << endl;
    }

    // Ignore errors and go without a pager unless the pager was specified
    // by the user.
    //
    try
    {
      // Redirect child's STDIN to a pipe.
      //
      p_ = process (args.data (), -1, 1, 2, nullptr /* cwd */, env);

      // Wait a bit and see if the pager has exited before reading anything
      // (e.g., because exec() couldn't find the program). If you know a
      // cleaner way to handle this, let me know (and no, a select()-based
      // approach doesn't work; the pipe is buffered and therefore is always
      // ready for writing).
      //
      // MINGW GCC 4.9 doesn't implement this_thread so use Win32 Sleep().
      //
#ifndef _WIN32
      this_thread::sleep_for (chrono::milliseconds (50));
#else
      Sleep (50);
#endif

      if (p_.try_wait ())
      {
        p_.out_fd.reset ();

        if (pager != nullptr)
          throw_generic_error (ECHILD);
      }
      else
        os_.open (move (p_.out_fd));
    }
    catch (const process_error& e)
    {
      if (e.child)
      {
        cerr << args[0] << ": unable to execute: " << e << endl;
        exit (1);
      }

      // Ignore unless it was a user-specified pager.
      //
      if (pager != nullptr)
        throw; // Re-throw as system_error.
    }

    // Setup the indentation machinery and the 'broken pipe' error handling.
    //
    // Note that the pager, if instructed to quit, may not bother reading out
    // all the data from stdin and just exit successfully (with 0 code)
    // outright. In this case writing to the pipe on our end results in the
    // EPIPE (EINVAL on Windows) error. We will handle this error by switching
    // into the skip mode, so that the caller may continue to write the data
    // normally till the end, without the need to handle this situation in a
    // special way.
    //
    buf_ = stream ().rdbuf (this);
  }

  bool pager::
  wait (bool ie)
  {
    // Prevent ofdstream::close() from throwing in the ignore errors mode.
    //
    if (ie)
      os_.exceptions (ofdstream::goodbit);

    os_.close ();

    // Teardown the indentation machinery and the 'broken pipe' error handling.
    //
    // Note that we need to do this after the above close() call, so that the
    // underlying flush() call would still be noop in the skip mode.
    //
    if (buf_ != nullptr)
    {
      stream ().rdbuf (buf_);
      buf_ = nullptr;
    }

    return p_.wait (ie);
  }

  pager::int_type pager::
  overflow (int_type c)
  {
    // Unless we are in the skip mode or the passed character is EOF, print
    // the indentation, if required, and delegate writing the character to the
    // stashed ofdstream's streambuf. Switch to the skip mode on the 'broken
    // pipe' error (see above for details).
    //
    if (!skip_ && c != traits_type::eof ())
    try
    {
      if (!indent_.empty ())
      {
        if (prev_ == '\n' && c != '\n') // Don't indent blanks.
        {
          auto n (static_cast<streamsize> (indent_.size ()));

          if (buf_->sputn (indent_.c_str (), n) != n)
            return traits_type::eof ();
        }

        prev_ = c;
      }

      return buf_->sputc (c);
    }
    //
    // Note that we catch the system_error exception rather than
    // ios_base::failure to be compilable with older C++ standard libraries
    // which may not derive ios_base::failure from system_error (GCC 4.9,
    // etc). For such libraries there is no easy way to handle the 'broken
    // pipe' error at this level since it is indistinguishable from other IO
    // errors. Thus, we just pass it through. Also note that we could
    // potentially treat any IO error as the 'broken pipe' error for such
    // libraries (it's actually hard to think of other reasons). Let's,
    // however, keep it simple for now and see how it goes.
    //
    catch (const system_error& e)
    {
      if (e.code ().category () == generic_category () &&
#ifndef _WIN32
          e.code ().value () == EPIPE)
#else
          e.code ().value () == EINVAL)
#endif
      {
        skip_ = true;

        // Fall through.
      }
      else
        throw;
    }

    return 0; // Success.
  }

  int pager::
  sync ()
  {
    if (!skip_)
    try
    {
      return buf_->pubsync ();
    }
    catch (const system_error& e)
    {
      if (e.code ().category () == generic_category () &&
#ifndef _WIN32
          e.code ().value () == EPIPE)
#else
          e.code ().value () == EINVAL)
#endif
      {
        skip_ = true;

        // Fall through.
      }
      else
        throw;
    }

    return 0; // Success.
  }
}
