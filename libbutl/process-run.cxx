// file      : libbutl/process-run.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules_ts
#include <libbutl/process.mxx>
#endif

// C includes.

#ifndef __cpp_lib_modules_ts
#include <cstdlib>  // exit()
#include <iostream> // cerr
#endif

// Other includes.

#ifdef __cpp_modules_ts
module butl.process;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules_ts
import std.core;
import std.io;
#endif
import butl.path;
#endif

import butl.utility; // operator<<(ostream,exception)
#else
#include <libbutl/utility.mxx>
#endif

using namespace std;

namespace butl
{
  process
  process_start (const dir_path* cwd,
                 const process_path& pp,
                 const char* cmd[],
                 const char* const* envvars,
                 process::pipe in,
                 process::pipe out,
                 process::pipe err)
  {
    try
    {
      return process (pp, cmd,
                      in, out, err,
                      cwd != nullptr ? cwd->string ().c_str () : nullptr,
                      envvars);
    }
    catch (const process_child_error& e)
    {
      cerr << "unable to execute " << cmd[0] << ": " << e << endl;
      exit (1);
    }
  }
}
