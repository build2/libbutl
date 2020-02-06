// file      : tests/builtin/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <string>
#include <vector>
#include <utility>  // move()
#include <ostream>
#include <iostream>
#endif

// Other includes.

#ifdef __cpp_modules_ts
#ifdef __cpp_lib_modules_ts
import std.core;
import std.io;
#endif
import butl.path;
import butl.utility;   // eof()
import butl.builtin;
import butl.optional;
import butl.timestamp; // to_stream(duration)
#else
#include <libbutl/path.mxx>
#include <libbutl/utility.mxx>
#include <libbutl/builtin.mxx>
#include <libbutl/optional.mxx>
#include <libbutl/timestamp.mxx>
#endif

using namespace std;
using namespace butl;

inline ostream&
operator<< (ostream& os, const path& p)
{
  return os << p.representation ();
}

// Usage: argv[0] [-d <dir>] [-o <opt>] [-c] [-i] <builtin> <builtin-args>
//
// Execute the builtin and exit with its exit status.
//
// -d <dir>  use as a current working directory
// -c        use callbacks that, in particular, trace calls to stdout
// -o <opt>  additional builtin option recognized by the callback
// -i        read lines from stdin and append them to the builtin arguments
//
int
main (int argc, char* argv[])
{
  using butl::optional;

  cin.exceptions  (ios::badbit);
  cout.exceptions (ios::failbit | ios::badbit);
  cerr.exceptions (ios::failbit | ios::badbit);

  bool in (false);
  dir_path cwd;
  string option;
  builtin_callbacks callbacks;

  string name;
  vector<string> args;

  auto flag = [] (bool v) {return v ? "true" : "false";};

  // Parse the driver options and arguments.
  //
  int i (1);
  for (; i != argc; ++i)
  {
    string a (argv[i]);

    if (a == "-d")
    {
      ++i;

      assert (i != argc);
      cwd = dir_path (argv[i]);
    }
    else if (a == "-o")
    {
      ++i;

      assert (i != argc);
      option = argv[i];
    }
    else if (a == "-c")
    {
      callbacks = builtin_callbacks (
        [&flag] (const path& p, bool pre)
        {
          cout << "create " << p << ' ' << flag (pre) << endl;
        },
        [&flag] (const path& from, const path& to, bool force, bool pre)
        {
          cout << "move " << from << ' ' << to << ' ' << flag (force) << ' '
                          << flag (pre) << endl;
        },
        [&flag] (const path& p, bool force, bool pre)
        {
          cout << "remove " << p << ' ' << flag (force) << ' ' << flag (pre)
                            << endl;
        },
        [&option] (const vector<string>& args, size_t i)
        {
          cout << "option " << args[i] << endl;
          return !option.empty () && args[i] == option ? 1 : 0;
        },
        [] (const duration& d)
        {
          cout << "sleep ";
          to_stream (cout, d, false /* nanoseconds */);
          cout << endl;
        }
      );
    }
    else if (a == "-i")
      in = true;
    else
      break;
  }

  // Parse the builtin name and arguments.
  //
  assert (i != argc);
  name = argv[i++];

  for (; i != argc; ++i)
    args.push_back (argv[i]);

  // Read out additional arguments from stdin.
  //
  if (in)
  {
    string s;
    while (!eof (getline (cin, s)))
      args.push_back (move (s));
  }

  // Execute the builtin.
  //
  builtin_function* bf (builtins.find (name));

  if (bf == nullptr)
  {
    cerr << "unknown builtin '" << name << "'" << endl;
    return 1;
  }

  uint8_t r; // Storage.
  builtin b (bf (r, args, nullfd, nullfd, nullfd, cwd, callbacks));
  return b.wait ();
}
