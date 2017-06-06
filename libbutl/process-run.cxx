// file      : libbutl/process-run.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <libbutl/process.hxx>

#include <cstdlib>  // exit()
#include <iostream> // cerr

using namespace std;

namespace butl
{
  process
  process_start (const dir_path* cwd,
                 const process_path& pp,
                 const char* cmd[],
                 const char* const* envvars,
                 int in,
                 int out,
                 int err)
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
