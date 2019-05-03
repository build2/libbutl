// file      : tests/string-parser/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <string>
#include <vector>
#include <iostream>
#endif

// Other includes.

#ifdef __cpp_modules_ts
#ifdef __cpp_lib_modules_ts
import std.core;
import std.io;
#endif
import butl.utility;       // operator<<(ostream,exception)
import butl.string_parser;
#else
#include <libbutl/utility.mxx>
#include <libbutl/string-parser.mxx>
#endif

using namespace std;
using namespace butl::string_parser;

// Usage: argv[0] [-l] [-u] [-p]
//
// Read and parse lines into strings from STDIN and print them to STDOUT.
//
// -l  output each string on a separate line
// -u  unquote strings
// -p  output positions
//
int
main (int argc, char* argv[])
try
{
  bool spl (false);     // Print string per line.
  bool unquote (false);
  bool pos (false);

  for (int i (1); i != argc; ++i)
  {
    string o (argv[i]);

    if (o == "-l")
      spl = true;
    else if (o == "-u")
      unquote = true;
    else if (o == "-p")
      pos = true;
    else
      assert (false);
  }

  // Do not throw when eofbit is set (end of stream reached), and when failbit
  // is set (getline() failed to extract any character).
  //
  cin.exceptions  (ios::badbit);

  cout.exceptions (ios::failbit | ios::badbit);

  string l;
  while (getline (cin, l))
  {
    vector<pair<string, size_t>> v (parse_quoted_position (l, unquote));

    if (!spl)
    {
      for (auto b (v.cbegin ()), i (b), e (v.cend ()); i != e; ++i)
      {
        if (i != b)
          cout << ' ';

        if (pos)
          cout << i->second << ":";

        cout << i->first;
      }

      cout << endl;
    }
    else
    {
      for (const auto& s: v)
      {
        if (pos)
          cout << s.second << ":";

        cout << s.first << endl;
      }
    }
  }

  return 0;
}
catch (const invalid_string& e)
{
  cerr << e.position << ": " << e << endl;
  return 1;
}
