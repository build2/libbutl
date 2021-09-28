// file      : tests/process-term/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef _WIN32
#  include <time.h>
#  include <signal.h>
#  include <unistd.h>
#  include <sys/types.h>
#else
#  include <libbutl/win32-utility.hxx>
#endif

#include <string>
#include <cerrno>   // ERANGE
#include <utility>  // move()
#include <cstdlib>  // atexit(), exit(), strtoull()
#include <cstring>  // memset()
#include <cstdint>  // uint64_t
#include <iostream>
#ifndef _WIN32
#  include <chrono>
#endif

#include <libbutl/process.hxx>
#include <libbutl/optional.hxx>
#include <libbutl/fdstream.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

void
atexit_func ()
{
  cout << "exiting";
}

#ifndef _WIN32

volatile sig_atomic_t term_sig = 0;

static void
term (int sig)
{
  term_sig = sig;
}
#endif

// Usages:
//
// argv[0]
// argv[0] -s <sec> [-t (ignore|exit|default)] [-e] [-c <num>]
//
// In the first form run some basic process termination tests, running its
// child in the second form.
//
// In the second form optionally register the SIGTERM signal handler
// (POSIX-only) and the atexit function, then sleep for the requested number
// of seconds and exit with the specified status.
//
// -s <sec>
//    Sleep for the specified timeout.
//
// -t (ignore|exit|default)
//    Register the SIGTERM signal handler. If the signal is received than
//    either ignore it, interrupt the sleep and exit, or call the default
//    handler.
//
// -e
//    Register the function with atexit() that prints the 'exiting' string to
//    stdout.
//
// -c <num>
//    Exit with the specified status (zero by default).
//
int
main (int argc, const char* argv[])
{
  using butl::optional;

  auto num = [] (const string& s)
  {
    assert (!s.empty ());

    char* e (nullptr);
    errno = 0; // We must clear it according to POSIX.
    uint64_t r (strtoull (s.c_str (), &e, 10)); // Can't throw.
    assert (errno != ERANGE && e == s.c_str () + s.size ());

    return r;
  };

  int ec (0);
  optional<uint64_t> sec;

#ifndef _WIN32
  enum class sig_action
  {
    ignore,
    exit,
    default_
  };

  optional<sig_action> term_action;

  struct sigaction def_handler;
#endif

  for (int i (1); i != argc; ++i)
  {
    string o (argv[i]);

    if (o == "-s")
    {
      assert (++i != argc);
      sec = num (argv[i]);
    }
    else if (o == "-c")
    {
      assert (++i != argc);
      ec = static_cast<int> (num (argv[i]));
    }
    else if (o == "-e")
    {
      assert (atexit (atexit_func) == 0);
    }
    else if (o == "-t")
    {
      assert (++i != argc);

#ifndef _WIN32
      string v (argv[i]);

      if (v == "ignore")
        term_action = sig_action::ignore;
      else if (v == "exit")
        term_action = sig_action::exit;
      else if (v == "default")
        term_action = sig_action::default_;
      else
        assert (false);

      struct sigaction action;
      memset (&action, 0, sizeof (action));
      action.sa_handler = term;
      assert (sigaction (SIGTERM, &action, &def_handler) == 0);
#endif
    }
    else
      assert (false);
  }

#ifndef _WIN32
  auto sleep = [&term_action, &def_handler] (uint64_t sec)
  {
    // Wait until timeout expires or SIGTERM is received and is not ignored.
    //
    for (timespec tm {static_cast<time_t> (sec), 0};
         nanosleep (&tm, &tm) == -1; )
    {
      assert (term_action && errno == EINTR && term_sig == SIGTERM);

      if (*term_action == sig_action::ignore)
        continue;

      if (*term_action == sig_action::default_)
      {
        assert (sigaction (term_sig, &def_handler, nullptr) == 0);
        kill (getpid (), term_sig);
      }

      break;
    }
  };
#else
  auto sleep = [] (uint64_t sec)
  {
    Sleep (static_cast<DWORD> (sec) * 1000);
  };
#endif

  // Child process.
  //
  if (sec)
  {
    if (*sec != 0)
      sleep (*sec);

    return ec;
  }

  // Main process.
  //

  // Return true if the child process has written the specified string to
  // stdout, represented by the reading end of the specified pipe.
  //
  auto test_out = [] (fdpipe&& pipe, const char* out)
  {
    pipe.out.close ();

    ifdstream is (move (pipe.in));
    bool r (is.read_text () == out);
    is.close ();
    return r;
  };

#ifndef _WIN32
  // Terminate a process with the default SIGTERM handler.
  //
  {
    fdpipe pipe (fdopen_pipe ());
    process p (process_start (0, pipe, 2, argv[0], "-s", 60, "-e"));

    sleep (3); // Give the child some time to initialize.
    p.term ();

    assert (test_out (move (pipe), ""));

    assert (!p.wait ());
    assert (p.exit);
    assert (!p.exit->normal ());
    assert (p.exit->signal () == SIGTERM);
  }

  // Terminate a process that exits on SIGTERM. Make sure it exits normally
  // and atexit function is called.
  //
  {
    fdpipe pipe (fdopen_pipe ());
    process p (process_start (0, pipe, 2,
                              argv[0], "-s", 60, "-t", "exit", "-e", "-c", 5));

    sleep (3); // Give the child some time to initialize.
    p.term ();

    assert (test_out (move (pipe), "exiting"));

    assert (!p.wait ());
    assert (p.exit);
    assert (p.exit->normal ());
    assert (p.exit->code () == 5);
  }

  // Terminate a process that calls the default handler on SIGTERM.
  //
  {
    fdpipe pipe (fdopen_pipe ());
    process p (
      process_start (0, pipe, 2,
                     argv[0], "-s", 60, "-t", "default", "-e", "-c", 5));

    sleep (3); // Give the child some time to initialize.
    p.term ();

    assert (test_out (move (pipe), ""));

    assert (!p.wait ());
    assert (p.exit);
    assert (!p.exit->normal ());
    assert (p.exit->signal () == SIGTERM);
  }

  // Terminate and then kill still running process.
  //
  {
    fdpipe pipe (fdopen_pipe ());
    process p (process_start (0, pipe, 2,
                              argv[0], "-s", 60, "-t", "ignore", "-e"));

    sleep (3); // Give the child some time to initialize.
    p.term ();

    assert (!p.timed_wait (chrono::seconds (1)));

    p.kill ();

    assert (test_out (move (pipe), ""));

    assert (!p.wait ());
    assert (p.exit);
    assert (!p.exit->normal ());
    assert (p.exit->signal () == SIGKILL);
  }

  // Terminate an already terminated process.
  //
  {
    fdpipe pipe (fdopen_pipe ());
    process p (process_start (0, pipe, 2, argv[0], "-s", 0, "-c", 5));

    sleep (4); // Give the child some time to terminate.
    p.term ();

    assert (test_out (move (pipe), ""));

    assert (!p.wait ());
    assert (p.exit);
    assert (p.exit->normal ());
    assert (p.exit->code () == 5);
  }

  // Terminate a process being terminated.
  //
  {
    fdpipe pipe (fdopen_pipe ());
    process p (process_start (0, pipe, 2, argv[0], "-s", 60));

    p.term ();
    p.term ();

    assert (test_out (move (pipe), ""));

    assert (!p.wait ());
    assert (p.exit);
    assert (!p.exit->normal ());
  }

  // Kill a process being terminated.
  //
  {
    fdpipe pipe (fdopen_pipe ());
    process p (process_start (0, pipe, 2, argv[0], "-s", 60));

    p.term ();
    p.kill ();

    assert (test_out (move (pipe), ""));

    assert (!p.wait ());
    assert (p.exit);
    assert (!p.exit->normal ());
    assert (p.exit->signal () == SIGTERM || p.exit->signal () == SIGKILL);
  }

  // Kill a process being killed.
  //
  {
    fdpipe pipe (fdopen_pipe ());
    process p (process_start (0, pipe, 2, argv[0], "-s", 60));

    p.kill ();
    p.kill ();

    assert (test_out (move (pipe), ""));

    assert (!p.wait ());
    assert (p.exit);
    assert (!p.exit->normal ());
  }
#endif

  // Terminate and wait a process.
  //
  {
    fdpipe pipe (fdopen_pipe ());
    process p (process_start (0, pipe, 2, argv[0], "-s", 60, "-e"));

    sleep (3); // Give the child some time to initialize.
    p.term ();

    assert (test_out (move (pipe), ""));

    assert (!p.wait ());
    assert (p.exit);
    assert (!p.exit->normal ());
  }

  // Kill and wait a process.
  //
  {
    fdpipe pipe (fdopen_pipe ());
    process p (process_start (0, pipe, 2, argv[0], "-s", 60, "-e"));

    sleep (3); // Give the child some time to initialize.
    p.kill ();

    assert (test_out (move (pipe), ""));

    assert (!p.wait ());
    assert (p.exit);
    assert (!p.exit->normal ());
  }

  // Kill a terminated process.
  //
  {
    fdpipe pipe (fdopen_pipe ());
    process p (process_start (0, pipe, 2, argv[0], "-s", 0, "-c", 5));

    sleep (4); // Give the child some time to terminate.
    p.kill ();

    assert (test_out (move (pipe), ""));

    assert (!p.wait ());
    assert (p.exit);
    assert (p.exit->normal ());
    assert (p.exit->code () == 5);
  }
}
