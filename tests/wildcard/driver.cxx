// file      : tests/wildcard/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <vector>
#include <cassert>
#include <iostream>
#include <algorithm> // sort()
#include <exception>

#include <butl/path>
#include <butl/utility>    // operator<<(ostream, exception)
#include <butl/filesystem>

using namespace std;
using namespace butl;

// Disable arguments globbing that may be enabled by default for MinGW runtime.
//
// Note that if _CRT_glob symbol is not defined explicitly, then runtime will
// be bound to the one defined in the implicitly linked libmingw32.a library.
// Yet another (but more troublesome) way is to link CRT_noglob.o (from MinGW
// libraries directory) that provides exactly the same symbol definition.
//
#ifdef __MINGW32__
int _CRT_glob = 0;
#endif

// Usage: argv[0] (-m <pattern> <name> | -s [-n] <pattern> [<dir>])
//
// Execute actions specified by -m or -s options. Exit with code 0 if succeed,
// 1 if fail, 2 on the underlying OS error (print error description to STDERR).
//
// -m
//    Match a name against the pattern.
//
// -s
//    Search for paths matching the pattern in the directory specified (absent
//    directory means the current one). Print the matching canonicalized paths
//    to STDOUT in the ascending order. Succeed if at least one matching path
//    is found. Note that this option must go first in the command line,
//
// -n
//    Do not sort paths found.
//
int
main (int argc, const char* argv[])
try
{
  assert (argc >= 2);

  string op (argv[1]);
  bool match (op == "-m");
  assert (match || op == "-s");

  if (match)
  {
    assert (argc == 4);

    string pattern (argv[2]);
    string name (argv[3]);
    return path_match (pattern, name) ? 0 : 1;
  }
  else
  {
    assert (argc >= 3);

    bool sort (true);
    int i (2);
    for (; i != argc; ++i)
    {
      string o (argv[i]);
      if (o == "-n")
        sort = false;
      else
        break; // End of options.
    }

    assert (i != argc); // Still need pattern.
    path pattern (argv[i++]);

    dir_path start;
    if (i != argc)
      start = dir_path (argv[i++]);

    assert (i == argc); // All args parsed,

    vector<path> paths;
    auto add = [&paths] (path&& p) -> bool
    {
      paths.emplace_back (move (p.canonicalize ()));
      return true;
    };

    path_search (pattern, add, start);

    if (sort)
      std::sort (paths.begin (), paths.end ());

    for (const auto& p: paths)
      cout << p.representation () << endl;

    return paths.empty () ? 1 : 0;
  }
}
catch (const invalid_path& e)
{
  cerr << e << ": " << e.path << endl;
  return 2;
}
catch (const exception& e)
{
  cerr << e << endl;
  return 2;
}
