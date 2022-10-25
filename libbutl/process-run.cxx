// file      : libbutl/process-run.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/process.hxx>

#include <cstdlib>  // exit()
#include <iostream> // cerr

#include <libbutl/utility.hxx> // operator<<(ostream,exception)

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
                      move (in), move (out), move (err),
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
