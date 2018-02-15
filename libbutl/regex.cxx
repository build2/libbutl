// file      : libbutl/regex.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules
#include <libbutl/regex.mxx>
#endif

// C includes.

#ifndef __cpp_lib_modules
#include <regex>
#include <string>

#include <ostream>
#include <sstream>
#include <stdexcept> // runtime_error
#if defined(_MSC_VER) && _MSC_VER <= 1913
#  include <cstring> // strstr()
#endif
#endif

// Other includes.

#ifdef __cpp_modules
module butl.regex;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules
import std.core;
import std.io;
import std.regex;
#endif
#endif

import butl.utility; // operator<<(ostream, exception)
#else
#include <libbutl/utility.mxx>
#endif

namespace std
{
  // Currently libstdc++ just returns the name of the exception (bug #67361).
  // So we check that the description contains at least one space character.
  //
  // While VC's description is meaningful, it has an undesired prefix that
  // resembles the following: 'regex_error(error_badrepeat): '. So we skip it.
  //
  ostream&
  operator<< (ostream& o, const regex_error& e)
  {
    const char* d (e.what ());

#if defined(_MSC_VER) && _MSC_VER <= 1913
    // Note: run the regex test like this to check new VC version:
    //
    // ./driver.exe a '{' b
    //
    const char* rd (strstr (d, "): "));
    if (rd != nullptr)
      d = rd + 3;
#endif

    ostringstream os;
    os << runtime_error (d); // Sanitize the description.

    string s (os.str ());
    if (s.find (' ') != string::npos)
      o << ": " << s;

    return o;
  }
}
