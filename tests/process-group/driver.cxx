// file      : tests/process-group/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef _WIN32
#  include <signal.h> // sigaction(), sigemptyset(), sigaddset(), raise(),
                      // sigwait(), pthread_sigmask(), pthread_kill(), SIG*,
                      // kill()
#  include <unistd.h> // getpid(), _exit()
#  include <string.h> // memset()

#  include <thread>
#  include <chrono>
#  include <algorithm> // copy()
#  include <exception> // terminate()
#else
#  include <libbutl/win32-utility.hxx>
#endif

#include <string>
#include <vector>
#include <utility>      // move()
#include <cerrno>       // ERANGE
#include <cstddef>      // size_t
#include <cstdint>      // uint64_t
#include <cstdlib>      // strtoull()
#include <cstring>      // strlen()
#include <iostream>     // cerr
#include <system_error>

#include <libbutl/path.hxx>
#include <libbutl/process.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

using strings = vector<string>;
using cstrings = vector<const char*>;

// NOTE: keep the signal handlers in sync with build2 driver.
//
#ifndef _WIN32
static void
terminating_signal_handler (int sig)
{
  for (process::handle_type g: process::groups)
    kill (-g, sig != SIGABRT ? sig : SIGKILL);

  struct sigaction sa;
  ::memset (&sa, 0, sizeof (sa));
  sigemptyset (&sa.sa_mask);
  sa.sa_handler = SIG_DFL;

  if (sigaction (sig, &sa, nullptr /* oldact */) != 0 || raise (sig) != 0)
    _exit (1);
}

static void
job_control_signal_handler (int sig)
{
  for (process::handle_type g: process::groups)
    kill (-g, sig);

  if (sig == SIGTSTP && raise (SIGSTOP) != 0)
    _exit (1);
}

extern "C" void
driver_signal_handler (int sig)
{
  switch (sig)
  {
  case SIGTERM:
  case SIGINT:
  case SIGQUIT:
  case SIGHUP:
  case SIGABRT:
    {
      terminating_signal_handler (sig);
      break;
    }
  case SIGTSTP:
  case SIGCONT:
    {
      job_control_signal_handler (sig);
      break;
    }
  default: assert (false);
  }
}

static sigset_t sigwait_mask;

static void*
sigwait_thread_function ()
{
  for (bool locked (false); true; )
  {
    int sig;
    if (int e = sigwait (&sigwait_mask, &sig))
    {
      cerr << "error: unable to wait for signals: "
           << system_error (e, generic_category ()) // Sanitize.
           << endl;

      terminate ();
    }

    if (sig == SIGUSR1) // Request to shutdown.
      break;

    switch (sig)
    {
    case SIGTERM:
    case SIGINT:
    case SIGQUIT:
    case SIGHUP:
    case SIGABRT:
      {
        // Unblock the signal, so that it can be raised.
        //
        sigset_t um;
        sigemptyset (&um);
        sigaddset (&um, sig);

        if (int e = pthread_sigmask (SIG_UNBLOCK, &um, nullptr /* oldset */))
        {
          cerr << "error: unable to unblock signal " << sig << ": "
               << system_error (e, generic_category ()) // Sanitize.
               << endl;

          terminate ();
        }

        if (!locked)
          process::mutex.lock_shared ();

        terminating_signal_handler (sig);
        break;
      }
    case SIGTSTP:
    case SIGCONT:
      {
        if (sig == SIGTSTP)
        {
          process::mutex.lock_shared ();
          locked = true;
        }

        job_control_signal_handler (sig);

        if (sig == SIGCONT)
        {
          process::mutex.unlock_shared ();
          locked = false;
        }

        break;
      }
    default: assert (false);
    }
  }

  return nullptr;
}
#endif

// Sleep for the specified number of milliseconds.
//
static inline void
sleep_ms (uint64_t ms)
{
  // MINGW GCC 4.9 doesn't implement this_thread so use Win32 Sleep().
  //
#ifndef _WIN32
  this_thread::sleep_for (chrono::milliseconds (ms));
#else
  Sleep (static_cast<DWORD> (ms));
#endif
}

// Parse the unsigned number.
//
static uint64_t
number (const char* s)
{
  assert (s != nullptr && *s != '\0');

  char* e (nullptr);
  errno = 0; // We must clear it according to POSIX.
  uint64_t r (strtoull (s, &e, 10)); // Can't throw.
  assert (errno != ERANGE && e == s + strlen (s));
  return r;
};

// Return the process information in the `<path> -c <arg>...`[ <exit-info>]
// form.
//
static string
child_to_string (const path& p,
                 const strings& as,
                 optional<process_exit> e = nullopt)
{
  string r ('`' + p.string () + " -c");

  for (const string& a: as)
    r += ' ' + a;

  r += '`';

  if (e)
    r += ' ' + to_string (*e);

  return r;
}

static bool
wait (process& pr, bool use_try_wait)
{
  if (!use_try_wait)
    return pr.wait ();

  while (true)
  {
    if (optional<bool> r = pr.try_wait ())
      return *r;

    sleep_ms (1);
  }
}

#ifndef _WIN32

// Convert a signal name to its numeric value.
//
static int
to_signal (const string& s)
{
  if (s      == "SIGKILL") return SIGKILL;
  else if (s == "SIGTERM") return SIGTERM;
  else if (s == "SIGINT")  return SIGINT;
  else if (s == "SIGQUIT") return SIGQUIT;
  else if (s == "SIGABRT") return SIGABRT;
  else if (s == "SIGHUP")  return SIGHUP;
  else if (s == "SIGSTOP") return SIGSTOP;
  else if (s == "SIGTSTP") return SIGTSTP;
  else if (s == "SIGCONT") return SIGCONT;
  else if (s == "SIGCHLD") return SIGCHLD;

  cerr << "error: unknown signal name " << s << endl;
  assert (false);
  return 0;
}

// Convert a signal numeric value to its name.
//
static string
signal_to_string (int sig)
{
  switch (sig)
  {
  case SIGKILL: return "SIGKILL";
  case SIGTERM: return "SIGTERM";
  case SIGINT:  return "SIGINT";
  case SIGQUIT: return "SIGQUIT";
  case SIGABRT: return "SIGABRT";
  case SIGHUP:  return "SIGHUP";
  case SIGSTOP: return "SIGSTOP";
  case SIGTSTP: return "SIGTSTP";
  case SIGCONT: return "SIGCONT";
  case SIGCHLD: return "SIGCHLD";
  default:      return "unknown signal " + to_string (sig);
  }

  assert (false);
  return "";
}

// Wait for a process to terminate and return true if the process was
// terminated abnormally on the specified signal. Otherwise, issue diagnostics
// and return false.
//
static bool
wait_signal (process& pr,
             int sig,
             const path& p,
             const strings& args,
             bool use_try_wait = false)
{
  try
  {
    bool r (!wait (pr, use_try_wait) &&
            !pr.exit->normal ()      &&
            pr.exit->signal () == sig);

    if (!r)
      cerr << "error: process should have terminated on signal " << sig << endl
           << "  info: " << child_to_string (p, args, *pr.exit) << endl;

    return r;
  }
  catch (const process_error& e)
  {
    cerr << "error: process reaping failed: " << e.what () << endl
         << "  info: " << child_to_string (p, args) << endl;
  }

  return false;
}
#endif

// Wait for a process to terminate and return true if the process was
// terminated normally with the specified exit code. Otherwise, issue
// diagnostics and return false.
//
static bool
wait_code (process& pr,
           uint64_t code,
           const path& p,
           const strings& args,
           bool use_try_wait = false)
{
  try
  {
    wait (pr, use_try_wait);

    bool r (pr.exit->normal () && pr.exit->code () == code);

    if (!r)
      cerr << "error: process should have exited with code "
           << static_cast<size_t> (code) << endl
           << "  info: " << child_to_string (p, args, *pr.exit) << endl;

    return r;
  }
  catch (const process_error& e)
  {
    cerr << "error: process reaping failed: " << e.what () << endl
         << "  info: " << child_to_string (p, args) << endl;
  }

  return false;
}

// Start a child process with the specified arguments (see the below usage
// description for details). On failure, issue diagnostics and return the
// already terminated process object.
//
static process
start (const path& p, const strings& as = {}, bool new_group = true)
{
  cstrings args {p.string ().c_str (), "-c"};

  for (const string& a: as)
    args.push_back (a.c_str ());

  args.push_back (nullptr);

  try
  {
    return process (args,
                    0 /* in */, 1 /* out */, 2 /* err */,
                    nullptr /* cwd */,
                    nullptr /* envvars */,
                    new_group);
  }
  catch (const process_error& e)
  {
    cerr << "error: process starting failed: " << e.what () << endl
         << "  info: " << child_to_string (p, as) << endl;
  }

  return process ();
}

// Return true, if the process was started successfully.
//
static inline bool
started (const process& pr)
{
  return pr.handle != 0;
}

// Execute the child process actions based on the specified arguments (see the
// below usage description for details).
//
// Note that the argv argument doesn't contain the program path and the -c, -G
// options.
//
static int
exec_child (const path& p, int argc, const char* argv[])
{
  for (int i (0); i != argc; ++i)
  {
    string o (argv[i]);

    if (o == "-s")      // Sleep the specified number of milliseconds?
    {
      assert (++i != argc);
      sleep_ms (number (argv[i]));
    }
    else if (o == "-e") // Exit with the specified code?
    {
      assert (++i != argc);
      return static_cast<int> (number (argv[i]));
    }
#ifndef _WIN32
    else if (o == "-k") // Kill itself?
    {
      assert (++i != argc);

      kill (getpid (), to_signal (argv[i]));
      return 0;
    }
    else if (o == "-g") // Kill the own process group?
    {
      assert (++i != argc);

      kill (0, to_signal (argv[i]));
      return 0;
    }
#endif
    else if (o == "{")  // Start the child process?
    {
      strings args;
      bool new_group (false);
      size_t level (0); // { ... } nesting level.

      assert (++i != argc);

      for (; i != argc; ++i)
      {
        string o (argv[i]);

        if (o == "{")
        {
          ++level;
        }
        else if (o[0] == '}' && (o[1] == '=' || o[1] == '~' || o[1] == '\0'))
        {
          if (level != 0)
          {
            --level;
          }
          else
          {
            process pr (start (p, args, new_group));
            assert (started (pr));

            switch (o[1])
            {
            case '=': // {...}=<code>
              {
                assert (wait_code (pr, number (o.c_str () + 2), p, args));
                break;
              }
            case '~': // {...}~<signal>
              {
#ifndef _WIN32
                assert (wait_signal (pr, to_signal (o.c_str () + 2), p, args));
#else
                assert (false);
#endif
                break;
              }
            default:  // {...}
              {
                pr.detach ();
                break;
              }
            }

            break; // for(..)
          }
        }
        else if (level == 0)
        {
          if (o == "-G")
          {
            new_group = true;
            continue;
          }
        }

        args.push_back (move (o));
      }

      assert (i != argc);
    }
    else
      assert (false);
  }

  return 0;
}

static int
exec_tests (const path& p)
{
  // Note that to test both wait() and try_wait(), we normally will start 2
  // exemplars of a group lead to reap it using different functions.
  //

  // Group leader doesn't start any new group members and exits normally.
  //
  {
    strings args ({"-e", "3"});

    process pr1 (start (p, args));
    assert (started (pr1));

    process pr2 (start (p, args));
    assert (started (pr2));

    assert (wait_code (pr2, 3, p, args, true /* use_try_wait */));
    assert (wait_code (pr1, 3, p, args));
  }

  // Group leader starts new group members, recursively, which are all get
  // reaped with their original parents.
  //
  {
    strings args ({"{", "{", "}=0", "-e", "3", "}=3"});

    process pr1 (start (p, args));
    assert (started (pr1));

    process pr2 (start (p, args));
    assert (started (pr2));

    assert (wait_code (pr2, 0, p, args, true /* use_try_wait */));
    assert (wait_code (pr1, 0, p, args));
  }

#ifndef _WIN32
  // Make sure SIGCHLD, which we use to indicate unreaped members of the
  // process group, is ignored by default.
  //
  {
    strings args ({"-s", "5000", "-e", "3"});

    process pr1 (start (p, args));
    assert (started (pr1));

    process pr2 (start (p, args));
    assert (started (pr2));

    assert (kill (pr1.handle, SIGCHLD) == 0);
    assert (kill (pr2.handle, SIGCHLD) == 0);

    assert (wait_code (pr2, 3, p, args, true /* use_try_wait */));
    assert (wait_code (pr1, 3, p, args));
  }

  // Terminate a group leader, which doesn't start any new group members, with
  // different signals.
  //
  {
    vector<int> ss ({SIGTERM, SIGINT, SIGHUP, SIGKILL});
    strings args ({"-s", "5000"});

    for (int s: ss)
    {
      process pr1 (start (p, args));
      assert (started (pr1));

      process pr2 (start (p, args));
      assert (started (pr2));

      assert (kill (pr1.handle, s) == 0);
      assert (kill (pr2.handle, s) == 0);

      assert (wait_signal (pr2, s, p, args, true /* use_try_wait */));
      assert (wait_signal (pr1, s, p, args));
    }
  }

  // Terminate a whole group with different signals.
  //
  {
    vector<int> ss ({SIGTERM, SIGINT, SIGHUP, SIGKILL});

    // Process starts 2 detached children and waits for 10 sec before exit.
    // Each child starts 1 detached child and wait for 10 sec before exit.
    // Leaf children just wait for 10 sec before exit.
    //
    strings args ({
        "{", "{", "-s", "10000", "}", "-s", "10000", "}",
        "{", "{", "-s", "10000", "}", "-s", "10000", "}",
        "-s", "10000"});

    for (int s: ss)
    {
      process pr1 (start (p, args));
      assert (started (pr1));

      process pr2 (start (p, args));
      assert (started (pr2));

      assert (kill (-pr1.group, s) == 0);
      assert (kill (-pr2.group, s) == 0);

      assert (wait_signal (pr2, s, p, args, true /* use_try_wait */));
      assert (wait_signal (pr1, s, p, args));
    }
  }

  // As above but the group is killed by a group member.
  //
  {
    vector<int> ss ({SIGTERM, SIGINT, SIGHUP, SIGKILL});

    for (int s: ss)
    {
      strings args ({
        "{", "{", "-s", "5000", "-g", signal_to_string (s), "}", "-s", "10000", "}",
        "{", "{", "-s", "10000", "}", "-s", "10000", "}",
        "-s", "10000"});

      process pr1 (start (p, args));
      assert (started (pr1));

      process pr2 (start (p, args));
      assert (started (pr2));

      assert (wait_signal (pr2, s, p, args, true /* use_try_wait */));
      assert (wait_signal (pr1, s, p, args));
    }
  }

  // Group leader starts the detached long-running child and exits.
  //
  {
    strings args ({"{", "-s", "5000", "}"});

    process pr1 (start (p, args));
    assert (started (pr1));

    process pr2 (start (p, args));
    assert (started (pr2));

    assert (wait_signal (pr2, SIGCHLD, p, args, true /* use_try_wait */));
    assert (wait_signal (pr1, SIGCHLD, p, args));
  }

  // Group leader starts the child, which starts the detached long-running
  // child and exits. The group lead reaps its child.
  //
  {
    strings args ({"{", "{", "-s", "5000", "}", "}=0"});

    process pr1 (start (p, args));
    assert (started (pr1));

    process pr2 (start (p, args));
    assert (started (pr2));

    assert (wait_signal (pr2, SIGCHLD, p, args, true /* use_try_wait */));
    assert (wait_signal (pr1, SIGCHLD, p, args));
  }

  // Note that under the load the terminated child can be reaped by the init
  // process (or alike) much faster than we can notice that this child has
  // ever existed (see process::wait() for details).
  //
#if 0
  // Group leader starts the detached child and exits after the child has
  // terminated normally.
  //
  {
    strings args ({"{", "}", "-s", "5000"});

    process pr1 (start (p, args));
    assert (started (pr1));

    process pr2 (start (p, args));
    assert (started (pr2));

    assert (wait_signal (pr2, SIGCHLD, p, args, true /* use_try_wait */));
    assert (wait_signal (pr1, SIGCHLD, p, args));
  }

  // Group leader starts the detached child and exits after the child has
  // terminated on SIGTERM.
  //
  {
    strings args ({"{", "-k", "SIGTERM", "}", "-s", "5000"});

    process pr1 (start (p, args));
    assert (started (pr1));

    process pr2 (start (p, args));
    assert (started (pr2));

    assert (wait_signal (pr2, SIGCHLD, p, args, true /* use_try_wait */));
    assert (wait_signal (pr1, SIGCHLD, p, args));
  }
#endif

  // Group leader starts the new detached group members and exits. We reap the
  // leader long after these group members are terminated (normally and not).
  //
  // Note that after the leader exits, its children get adopted by the init
  // (or alike) process. Since we reap the leader long after its children has
  // terminated and were already reaped by init, we don't notice that the
  // group leader didn't reap the group members.
  //
  {
    strings args ({"{", "-s", "1000", "}",
                   "{", "-s", "1000", "-k", "SIGTERM", "}",
                   "{", "-s", "1000", "-k", "SIGKILL", "}",
                   "{", "-s", "1000", "-k", "SIGTSTP", "}"});

    process pr1 (start (p, args));
    assert (started (pr1));

    process pr2 (start (p, args));
    assert (started (pr2));

    sleep_ms (5000); // Wait until grandchildren are dead/stopped.

    // Awake the stopped grandchildren.
    //
    assert (kill (-pr1.group, SIGCONT) == 0);
    assert (kill (-pr2.group, SIGCONT) == 0);

    sleep_ms (3000); // Wait until the awaken grandchildren has exited.

    // We shouldn't notice any unreaped grandchildren.
    //
    assert (wait_code (pr2, 0, p, args, true /* use_try_wait */));
    assert (wait_code (pr1, 0, p, args));
  }

  // Test the signal forwarding in the signal handlers.
  //

  // Group leader starts another group leader and waits/reaps it. We terminate
  // them, recursively.
  //
  {
    strings args1 ({"{", "-G", "-s", "10000", "}=0"});

    process pr1 (start (p, args1));
    assert (started (pr1));

    process pr2 (start (p, args1));
    assert (started (pr2));

    // As above, but use the sigwait() based signals handling.
    //
    strings args2 ({"-S", "{", "-S", "-G", "-s", "10000", "}=0"});

    process pr3 (start (p, args2));
    assert (started (pr3));

    process pr4 (start (p, args2));
    assert (started (pr4));

    sleep_ms (3000);

    assert (kill (-pr1.group, SIGTERM) == 0);
    assert (kill (-pr2.group, SIGTERM) == 0);
    assert (kill (-pr3.group, SIGTERM) == 0);
    assert (kill (-pr4.group, SIGTERM) == 0);

    assert (wait_signal (pr4, SIGTERM, p, args2, true /* use_try_wait */));
    assert (wait_signal (pr2, SIGTERM, p, args1, true /* use_try_wait */));
    assert (wait_signal (pr1, SIGTERM, p, args1));
    assert (wait_signal (pr3, SIGTERM, p, args2));
  }

  // Group leader starts another group leader and waits/reaps it. We suspend
  // and resume them, recursively.
  //
  {
    strings args1 ({"{", "-G", "-s", "10000", "}=0"});

    process pr1 (start (p, args1));
    assert (started (pr1));

    process pr2 (start (p, args1));
    assert (started (pr2));

    // As above, but use the sigwait() based signals handling.
    //
    strings args2 ({"-S", "{", "-S", "-G", "-s", "10000", "}=0"});

    process pr3 (start (p, args1));
    assert (started (pr3));

    process pr4 (start (p, args1));
    assert (started (pr4));

    sleep_ms (3000);

    assert (kill (-pr1.group, SIGTSTP) == 0);
    assert (kill (-pr2.group, SIGTSTP) == 0);
    assert (kill (-pr3.group, SIGTSTP) == 0);
    assert (kill (-pr4.group, SIGTSTP) == 0);

    sleep_ms (3000);

    assert (kill (-pr1.group, SIGCONT) == 0);
    assert (kill (-pr2.group, SIGCONT) == 0);
    assert (kill (-pr3.group, SIGCONT) == 0);
    assert (kill (-pr4.group, SIGCONT) == 0);

    assert (wait_code (pr4, 0, p, args2, true /* use_try_wait */));
    assert (wait_code (pr2, 0, p, args1, true /* use_try_wait */));
    assert (wait_code (pr1, 0, p, args1));
    assert (wait_code (pr3, 0, p, args2));
  }

  // Group leader starts another group leader and waits/reaps it. We suspend,
  // terminate, and resume them, recursively.
  //
  {
    strings args1 ({"{", "-G", "-s", "10000", "}=0"});

    process pr1 (start (p, args1));
    assert (started (pr1));

    process pr2 (start (p, args1));
    assert (started (pr2));

    // As above, but use the sigwait() based signals handling.
    //
    strings args2 ({"-S", "{", "-S", "-G", "-s", "10000", "}=0"});

    process pr3 (start (p, args1));
    assert (started (pr3));

    process pr4 (start (p, args1));
    assert (started (pr4));

    sleep_ms (3000);

    assert (kill (-pr1.group, SIGTSTP) == 0);
    assert (kill (-pr2.group, SIGTSTP) == 0);
    assert (kill (-pr3.group, SIGTSTP) == 0);
    assert (kill (-pr4.group, SIGTSTP) == 0);

    sleep_ms (3000);

    pr1.term ();
    pr2.term ();
    pr3.term ();
    pr4.term ();

    assert (kill (-pr1.group, SIGCONT) == 0);
    assert (kill (-pr2.group, SIGCONT) == 0);
    assert (kill (-pr3.group, SIGCONT) == 0);
    assert (kill (-pr4.group, SIGCONT) == 0);

    assert (wait_signal (pr4, SIGTERM, p, args2, true /* use_try_wait */));
    assert (wait_signal (pr2, SIGTERM, p, args1, true /* use_try_wait */));
    assert (wait_signal (pr1, SIGTERM, p, args1));
    assert (wait_signal (pr3, SIGTERM, p, args2));
  }
#endif

  return 0;
}

// Usages:
//
// argv[0]
// argv[0] -c <opts> [{ [-G] <arg>... }(=<code>|~<signal>)?]...
//
// In the first form run some basic process group tests, running child
// processes as process group leaders using the second form. The second form
// implements a microlanguage which instructs a child process to perform a
// list of actions in the specific order, such as: sleep, run another child,
// send a signal to itself or its group, exit with the specified code.
//
// The second form options:
//
// -c
//    Run the process as a child, performing the requested actions.
//
// -s <msec>
//    Sleep for the specified number of milliseconds.
//
// -e <num>
//    Exit with the specified code.
//
// -k <name>
//    Send the specified signal to itself.
//
// -g <name>
//    Send the specified signal to the own group.
//
// -S
//    Use sigwait() to read pending signals instead of installing custom
//    signal handlers.
//
// -G
//    Start the child process as a group leader.
//
//    Note that this is a pseudo-option since the child process don't receive
//    it as an argument. It is used (and removed from the arguments list) by
//    the parent process which spawns the child.
//
// The sequence of arguments '{' <arg>... '}(=<code>|~<signal>)?', where
// <arg>... may contain the above options and other {...} sequences, starts
// the child process `driver -c <arg>...`. If the closing '}' is followed by
// the exit code number or the signal name, then the process is reaped and the
// code/signal is verified. Otherwise, the process is detached (the original
// parent doesn't reap it). For example:
//
// driver -c { -s 10 } -s 1 { -s 10 { -s 5 } -e 3 }=3 -k SIGTERM
//
// The above child runs the detached child, waits 1ms, runs another child,
// reaps it, verifies that its exit code is 3, and send itself SIGTERM. The
// first grandchild sleeps for 10ms and exit with code 0. The second
// grandchild sleeps for 10ms, starts its own detached child which sleeps for
// 5ms, and exits with code 3.
//
int
main (int argc, const char* argv[])
{
  assert (argc == 1 ||               // Root test process.
          string (argv[1]) == "-c"); // Child process.

  // Initialize signal handling.
  //
#ifndef _WIN32
  thread sigwait_thread;
  sigset_t prev_signal_mask;

  // Process the immediate -S options and remove them from the arguments.
  //
  bool use_sigwait (false);
  {
    size_t level (0); // { ... } nesting level.

    for (int i (1); i != argc; )
    {
      string o (argv[i]);

      if (o == "-S")
      {
        if (level == 0)
        {
          use_sigwait = true;

          // Remove the option from the arguments.
          //
          copy (argv + i + 1, argv + argc, argv + i);
          --argc;
          continue;
        }
      }
      else if (o == "{")
      {
        ++level;
      }
      else if (o[0] == '}' && (o[1] == '=' || o[1] == '~' || o[1] == '\0'))
      {
        assert (level-- != 0);
      }

      ++i;
    }
  }

  if (!use_sigwait)
  {
    struct sigaction sa;
    ::memset (&sa, 0, sizeof (sa));
    sigemptyset (&sa.sa_mask);
    sa.sa_handler = driver_signal_handler;

    sigaction (SIGINT,  &sa, nullptr /* oldact */);
    sigaction (SIGTERM, &sa, nullptr /* oldact */);
    sigaction (SIGHUP,  &sa, nullptr /* oldact */);
    sigaction (SIGQUIT, &sa, nullptr /* oldact */);
    sigaction (SIGABRT, &sa, nullptr /* oldact */);

    sigaction (SIGTSTP, &sa, nullptr /* oldact */);
    sigaction (SIGCONT, &sa, nullptr /* oldact */);
  }
  else
  {
    // Block the signals which we plan to handle, to make sure they are
    // handled with the dedicated thread.
    //
    sigemptyset (&sigwait_mask);

    sigaddset (&sigwait_mask, SIGINT);
    sigaddset (&sigwait_mask, SIGTERM);
    sigaddset (&sigwait_mask, SIGHUP);
    sigaddset (&sigwait_mask, SIGQUIT);
    sigaddset (&sigwait_mask, SIGABRT);

    sigaddset (&sigwait_mask, SIGTSTP);
    sigaddset (&sigwait_mask, SIGCONT);

    sigaddset (&sigwait_mask, SIGUSR1);

    if (int e = pthread_sigmask (SIG_SETMASK,
                                 &sigwait_mask,
                                 &prev_signal_mask))
    {
      cerr << "unable to block signals: "
           << system_error (e, generic_category ()); // Sanitize.
      return 1;
    }

    sigwait_thread = thread (sigwait_thread_function);
  }
#endif

  // Run the tests/child.
  //
  path p (argv[0]);

  int r (argc != 1
         ? exec_child (p, argc - 2, argv + 2) // Skip program path and -c.
         : exec_tests (p));

  // Terminate signal handling.
  //
#ifndef _WIN32
  if (sigwait_thread.joinable ())
  {
    pthread_t h (sigwait_thread.native_handle ());

    if (int e = pthread_kill (h, SIGUSR1))
    {
      cerr << "error: unable to send terminating signal: "
           << system_error (e, generic_category ()) // Sanitize.
           << endl;

      return 1;
    }

    sigwait_thread.join ();

    // Unblock the previously blocked signals.
    //
    if (int e = pthread_sigmask (SIG_SETMASK,
                                 &prev_signal_mask,
                                 nullptr /* oldset */))
    {
      cerr << "error: unable to restore signal mask: "
           << system_error (e, generic_category ()) // Sanitize.
           << endl;

      return 1;
    }
  }
#endif

  return r;
}
