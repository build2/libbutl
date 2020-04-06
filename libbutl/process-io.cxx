// file      : libbutl/process-io.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules_ts
#include <libbutl/process-io.mxx>
#endif

// C includes.

#ifndef __cpp_lib_modules_ts
#include <ostream>

#include <cstring> // strchr()
#endif

// Other includes.

#ifdef __cpp_modules_ts
module butl.process_io;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules_ts
import std.core;
import std.io;
#endif
import butl.process;
#endif

import butl.path-io;
#else
#include <libbutl/path-io.mxx>
#endif

using namespace std;

namespace butl
{
  // process_env
  //
  ostream&
  operator<< (ostream& o, const process_env& env)
  {
    bool first (true);
    const dir_path* cwd (env.cwd);

    if (cwd != nullptr && !cwd->empty ())
    {
      if (cwd->string ().find (' ') != string::npos)
        o << "PWD=\"" << *cwd << '"';
      else
        o << "PWD=" << *cwd;

      first = false;
    }

    if (env.vars != nullptr)
    {
      for (const char* const* ev (env.vars); *ev != nullptr; ++ev)
      {
        if (first)
          first = false;
        else
          o << ' ';

        const char* v (*ev);

        // If there is no `=` in the string, then this is just a name
        // (variable unset) and we print it as the empty string assignment.
        //
        const char* eq (strchr (v, '='));

        // If there is the space character in the string, then we quote the
        // variable value, unless this is the variable name that contains the
        // space character in which case we quote the whole (potentially
        // broken) assignment.
        //
        const char* sp (strchr (v, ' '));

        if (eq != nullptr)             // Variable assignment?
        {
          if (sp == nullptr)           // No space?
          {
            o << v;
          }
          else if (eq < sp)            // Space in the value?
          {
            o.write (v, eq - v + 1);   // Name and '='.
            o << '"' << eq + 1 << '"'; // Quoted value.
          }
          else                         // Space in the name.
            o << '"' << v << '"';
        }
        else                           // Variable unset.
        {
          if (sp == nullptr)           // No space?
            o << v << '=';
          else                         // Space in the name.
            o << '"' << v << "=\"";
        }
      }
    }

    return o;
  }
}
