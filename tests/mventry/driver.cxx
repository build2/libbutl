// file      : tests/mventry/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <iostream>
#include <system_error>

#include <libbutl/path.hxx>
#include <libbutl/utility.hxx>    // operator<<(ostream, exception)
#include <libbutl/filesystem.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Usage: argv[0] <old-path> <new-path>
//
// Rename a file, directory or symlink or move it to the specified directory.
// For the later case the destination path must have a trailing directory
// separator. If succeed then exits with the zero code, otherwise prints the
// error descriptions and exits with the one code.
//
int
main (int argc, const char* argv[])
try
{
  assert (argc == 3);

  path from (argv[1]);
  path to   (argv[2]);

  cpflags fl (cpflags::overwrite_permissions | cpflags::overwrite_content);

  if (to.to_directory ())
    mventry_into (from, path_cast<dir_path> (move (to)), fl);
  else
    mventry (from, to, fl);

  return 0;
}
catch (const invalid_path& e)
{
  cerr << e << ": " << e.path << endl;
  return 1;
}
catch (const system_error& e)
{
  cerr << e << endl;
  return 1;
}
