// file      : tests/backtrace/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef _WIN32
#  include <sys/resource.h> // setrlimit()
#endif

#include <string>
#include <iostream>
#include <exception>    // set_terminate(), terminate_handler
#include <system_error>

#include <libbutl/process.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/backtrace.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

namespace test
{
  // Note: is not static to make sure the stack frame is not optimized out.
  //
  int
  func ()
  {
    throw system_error (EINVAL, generic_category ());
  }
}

static terminate_handler def_term_handler;

// Usages:
//
// argv[0] [-q]
// argv[0] -c [-b]
//
// In the first form run the child processes throwing unhandled exception,
// first of which sets up the backtrace-printing handler prior to throwing,
// and make sure that they terminate in the same way (abnormally or with the
// same exit status). Exit with the zero code if that's the case and the
// children terminated abnormally or with non-zero code and exit with the one
// code otherwise. Redirect stderr to /dev/null for the first child if
// requested (-q) and always for the second one.
//
// In the second form run as a child process that optionally sets up the
// backtrace-printing terminate handler (-b) and throws unhandled exception.
//
int
main (int argc, const char* argv[])
{
  bool child (false);
  bool backtrace (false);
  bool quiet (false);

  for (int i (1); i != argc; ++i)
  {
    string o (argv[i]);

    if (o == "-c")
    {
      child = true;
    }
    else if (o == "-b")
    {
      assert (child);
      backtrace = true;
    }
    else if (o == "-q")
    {
      assert (!child);
      quiet = true;
    }
    else
    {
      assert (false);
    }
  }

  // Run as a child.
  //
  if (child)
  {
    // Disable dumping core file on POSIX.
    //
#ifndef _WIN32
    struct rlimit rlim {0, 0};
    assert (setrlimit (RLIMIT_CORE, &rlim) == 0);
#endif

    if (backtrace)
    {
      def_term_handler = set_terminate ([]()
                                        {
                                          cerr << butl::backtrace ();

                                          if (def_term_handler != nullptr)
                                            def_term_handler ();
                                        });
    }

    return test::func ();
  }

  // Run as a parent.
  //
  auto_fd null (fdopen_null ());
  process_exit pe1 (process_run (0                       /* stdin */,
                                 1                       /* stdout */,
                                 quiet ? null.get () : 2 /* stderr */,
                                 argv[0], "-c", "-b"));

  if (pe1)
  {
    cerr << "error: child exited with zero code" << endl;
    return 1;
  }

  // Always run quiet.
  //
  process_exit pe2 (process_run (0    /* stdin */,
                                 1    /* stdout */,
                                 null /* stderr */,
                                 argv[0], "-c"));

  if (!(pe1.normal () == pe2.normal () &&
        (!pe1.normal () || pe1.code () == pe2.code ())))
  {
    cerr << "error: child processes terminated differently:" << endl <<
            "  info: with backtrace: "    << pe1 << endl <<
            "  info: without backtrace: " << pe2 << endl;

    return 1;
  }

  return 0;
}
