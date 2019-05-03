// file      : tests/regex/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <string>
#include <iostream>
#include <exception>
#endif

// Other includes.

#ifdef __cpp_modules_ts
#ifdef __cpp_lib_modules_ts
import std.core;
import std.io;
import std.regex; // @@ MOD TODO: shouldn't be necessary (re-export).
#endif
import butl.regex;
import butl.utility; // operator<<(ostream, exception)
#else
#include <libbutl/regex.mxx>
#include <libbutl/utility.mxx>
#endif

using namespace std;
using namespace butl;

// Usage: argv[0] [-ffo] [-fnc] [-m] <string> <regex> <format>
//
// Perform substitution of matched substrings with formatted replacement
// strings using regex_replace_*() functions. If the string matches the regex
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
// -m
//    Match the entire string, rather than its sub-strings.
//
int
main (int argc, const char* argv[])
try
{
  regex_constants::match_flag_type fl (regex_constants::match_default);

  int i (1);
  bool match (false);
  for (; i != argc; ++i)
  {
    string op (argv[i]);

    if (op == "-ffo")
      fl |= regex_constants::format_first_only;
    else if (op == "-fnc")
      fl |= regex_constants::format_no_copy;
    else if (op == "-m")
      match = true;
    else
      break;
  }

  assert (i + 3 == argc);

  string s   (argv[i++]);
  regex  re  (argv[i++]);
  string fmt (argv[i]);

  auto r (match
          ? regex_replace_match (s, re, fmt)
          : regex_replace_search (s, re, fmt, fl));

  if (r.second)
    cout << r.first << endl;

  return r.second ? 0 : 1;
}
catch (const regex_error& e)
{
  cerr << "invalid regex" << e << endl; // Print sanitized.
  return 2;
}
catch (const exception& e)
{
  cerr << e << endl;
  return 2;
}
