// file      : tests/process-run/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <iostream>

#include <libbutl/path.hxx>
#include <libbutl/process.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/small-vector.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

template <typename I,
          typename O,
          typename E,
          typename... A>
process_exit
run (I&& in,
     O&& out,
     E&& err,
     const process_env& env,
     A&&... args)
{
  return process_run (forward<I> (in),
                      forward<O> (out),
                      forward<E> (err),
                      env,
                      forward<A> (args)...);
}

int
main (int argc, const char* argv[])
{
  if (argc < 2) // No argument test.
    return 0;

  string a (argv[1]);

  if (a == "-c")
  {
    // -i read from stdin
    // -o write argument to stdout
    // -e write argument to stderr
    // -x exit with argument
    //
    for (int i (2); i != argc; ++i)
    {
      a = argv[i];

      if      (a == "-i") cin >> a;
      else if (a == "-o") cout << argv[++i] << endl;
      else if (a == "-e") cerr << argv[++i] << endl;
      else if (a == "-x") return atoi (argv[++i]);
    }

    return 0;
  }
  else
    assert (a == "-p");

  const string p (argv[0]);

  assert (run (0, 1, 2, p));
  assert (run (0, 1, 2, p, "-c"));

  process_run_callback ([] (const char* c[], size_t n)
                        {
                          process::print (cout, c, n);
                          cout << endl;
                        },
                        0, 1, 2,
                        p,
                        "-c");

  // Stream conversion and redirection.
  //
  assert (run (fdopen_null (), 1, 2, p, "-c", "-i"));
  assert (run (fdopen_null (), 2, 2, p, "-c", "-o", "abc"));
  assert (run (fdopen_null (), 1, 1, p, "-c", "-e", "abc"));

  {
    fdpipe pipe (fdopen_pipe ());
    process pr (process_start (pipe, process::pipe (-1, 1), 2, p, "-c", "-i"));
    pipe.close ();
    assert (pr.wait ());
  }

  // Argument conversion.
  //
  assert (run (0, 1, 2, p, "-c", "-o", "abc"));
  assert (run (0, 1, 2, p, "-c", "-o", string ("abc")));
  assert (run (0, 1, 2, p, "-c", "-o", path ("abc")));
  assert (run (0, 1, 2, p, "-c", "-o", 123));
}
