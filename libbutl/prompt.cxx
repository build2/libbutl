// file      : libbutl/prompt.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules_ts
#include <libbutl/prompt.mxx>
#endif

#ifndef __cpp_lib_modules_ts
#include <string>

#include <iostream>
#endif

// Other includes.

#ifdef __cpp_modules_ts
module butl.prompt;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules_ts
import std.core;
import std.io;
#endif
#endif

import butl.diagnostics;
#else
#include <libbutl/diagnostics.mxx> // diag_stream
#endif

using namespace std;

namespace butl
{
  bool
  yn_prompt (const string& prompt, char def)
  {
    // Writing a robust Y/N prompt is more difficult than one would expect.
    //
    string a;
    do
    {
      *diag_stream << prompt << ' ';

      // getline() will set the failbit if it failed to extract anything,
      // not even the delimiter and eofbit if it reached eof before seeing
      // the delimiter.
      //
      getline (cin, a);

      bool f (cin.fail ());
      bool e (cin.eof ());

      if (f || e)
        *diag_stream << endl; // Assume no delimiter (newline).

      if (f)
        throw ios_base::failure ("unable to read y/n answer from stdout");

      if (a.empty () && def != '\0')
      {
        // Don't treat eof as the default answer. We need to see the actual
        // newline.
        //
        if (!e)
          a = def;
      }
    } while (a != "y" && a != "n");

    return a == "y";
  }
}
