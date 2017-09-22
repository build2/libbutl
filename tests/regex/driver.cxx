// file      : tests/regex/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules
#include <string>
#include <iostream>
#include <exception>
#endif

// Other includes.

#ifdef __cpp_modules
#ifdef __cpp_lib_modules
import std.core;
import std.io;
#endif
import butl.regex;
import butl.utility; // operator<<(ostream, exception)
#else
#include <libbutl/regex.mxx>
#include <libbutl/utility.mxx>
#endif

using namespace std;
using namespace butl;

// Usage: argv[0] [-ffo] [-fnc] <string> <regex> <format>
//
// Perform substitution of matched substrings with formatted replacement
// strings using regex_replace_ex() function. If the string matches the regex
// then print the replacement to STDOUT and exit with zero code. Exit with
// code one if it doesn't match, and with code two on failure (print error
// description to STDERR).
//
// -ffo
//    Use format_first_only replacement flag.
//
// -fnc
//    Use format_no_copy replacement flag.
//
int
main (int argc, const char* argv[])
try
{
  regex_constants::match_flag_type fl (regex_constants::match_default);

  int i (1);
  for (; i != argc; ++i)
  {
    string op (argv[i]);

    if (op == "-ffo")
      fl |= regex_constants::format_first_only;
    else if (op == "-fnc")
      fl |= regex_constants::format_no_copy;
    else
      break;
  }

  assert (i + 3 == argc);

  string s   (argv[i++]);
  regex  re  (argv[i++]);
  string fmt (argv[i]);

  auto r (regex_replace_ex (s, re, fmt, fl));

  if (r.second)
    cout << r.first << endl;

  return r.second ? 0 : 1;
}
catch (const exception& e)
{
  cerr << e << endl;
  return 2;
}
