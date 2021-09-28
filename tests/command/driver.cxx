// file      : tests/command/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <ios>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>    // invalid_argument
#include <system_error>

#include <libbutl/path.hxx>
#include <libbutl/path-io.hxx>
#include <libbutl/process.hxx>
#include <libbutl/command.hxx>
#include <libbutl/utility.hxx>
#include <libbutl/optional.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Usages:
//
// argv[0] [-d <dir>] [-v <name>[=<value>]] [-s <name>=<value>] [-c <char>]
//         [-p] <command>
//
// argv[0] -C [-A] [-D] [-V <name>] [-S <status>] <arguments>
//
// In the first form run the specified command, changing the current
// directory, (re)setting the environment variables, performing substitutions,
// and printing the "expanded" command line, if requested.
//
// In the second form optionally print the program arguments, CWD, the
// environment variable values and exit with the status specified. This mode
// is normally used for the command being tested to dump the environment
// obtained from the caller.
//
// -d <dir>
//    Change the CWD for the command process.
//
// -v <name>[=<value>]
//    (Un)set the environment variable for the command process. Can be
//    specified multiple times.
//
// -s <name>=<value>
//    Perform command line substitutions using the specified variable. Can be
//    specified multiple times.
//
// -c <char>
//    Substitution symbol. The default is '@'.
//
// -p
//    Print the "expanded" command line.
//
// -C
//    Print the program arguments (-A), CWD (-D), environment variables (-V)
//    to stdout and exit with the status specifies (-S).
//
// -A
//    Print the program arguments to stdout.
//
// -D
//    Print the process CWD to stdout.
//
// -V <name>
//    Print the environment variable value (or <unset>) to stdout. Can be
//    specified multiple times.
//
// -S <status>
//    Exit with the specified status code.
//
int
main (int argc, const char* argv[])
{
  using butl::optional;
  using butl::getenv;

  // Parse and validate the arguments.
  //
  dir_path cwd;
  vector<const char*> vars;
  optional<command_substitution_map> substitutions;
  char subst ('@');
  optional<string> command;
  bool print (false);

  for (int i (1); i != argc; ++i)
  {
    string o (argv[i]);

    if (o == "-d")
    {
      assert (++i != argc);
      cwd = dir_path (argv[i]);
    }
    else if (o == "-v")
    {
      assert (++i != argc);
      vars.push_back (argv[i]);
    }
    else if (o == "-s")
    {
      assert (++i != argc);

      if (!substitutions)
        substitutions = command_substitution_map ();

      string v (argv[i]);
      size_t p (v.find ('='));

      assert (p != string::npos && p != 0);

      (*substitutions)[string (v, 0, p)] = string (v, p + 1);
    }
    else if (o == "-c")
    {
      assert (++i != argc);

      string v (argv[i]);

      assert (v.size () == 1);
      subst = v[0];
    }
    else if (o == "-p")
    {
      print = true;
    }
    else if (o == "-C")
    {
      assert (i == 1); // Must go first.

      int ec (0);
      bool print_cwd (false);

      // Include the program path into the arguments list.
      //
      vector<const char*> args ({argv[0]});

      for (++i; i != argc; ++i)
      {
        o = argv[i];

        if (o == "-A")
        {
          print = true;
        }
        else if (o == "-D")
        {
          print_cwd = true;
        }
        else if (o == "-V")
        {
          assert (++i != argc);
          vars.push_back (argv[i]);
        }
        else if (o == "-S")
        {
          assert (++i != argc);
          ec = stoi (argv[i]);
        }
        else
          args.push_back (argv[i]);
      }

      args.push_back (nullptr);

      if (print)
      {
        process::print (cout, args.data ());
        cout << endl;
      }

      if (print_cwd)
        cout << dir_path::current_directory () << endl;

      for (const auto& v: vars)
      {
        optional<string> vv (getenv (v));
        cout << (vv ? *vv : "<unset>") << endl;
      }

      return ec;
    }
    else
    {
      assert (!command);
      command = argv[i];
    }
  }

  assert (command);

  // Run the command.
  //
  try
  {
    optional<process_env> pe;

    if (!cwd.empty () || !vars.empty ())
      pe = process_env (process_path (), cwd, vars);

    process_exit e (command_run (*command,
                                 pe,
                                 substitutions,
                                 subst,
                                 [print] (const char* const args[], size_t n)
                                 {
                                   if (print)
                                   {
                                     process::print (cout, args, n);
                                     cout << endl;
                                   }
                                 }));

    if (!e)
      cerr << "process " << argv[0] << " " << e << endl;

    return e.normal () ? e.code () : 1;
  }
  catch (const invalid_argument& e)
  {
    cerr << e << endl;
    return 1;
  }
  catch (const ios::failure& e)
  {
    cerr << e << endl;
    return 1;
  }
  // Also handles process_error exception (derived from system_error).
  //
  catch (const system_error& e)
  {
    cerr << e << endl;
    return 1;
  }

  return 0;
}
