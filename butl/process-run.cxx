// file      : butl/process-run.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/process>

#include <cstdlib>  // exit()
#include <iostream> // cerr

using namespace std;

namespace butl
{
  process
  process_start (const dir_path& cwd,
                 const process_path& pp,
                 const char* cmd[],
                 int in,
                 int out,
                 int err)
  {
    try
    {
      return process (cwd.string ().c_str (), pp, cmd, in, out, err);
    }
    catch (const process_child_error& e)
    {
      cerr << "unable to execute " << cmd[0] << ": " << e << endl;
      exit (1);
    }
  }
}
