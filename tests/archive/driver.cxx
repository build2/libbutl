// file      : tests/archive/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/path.hxx>
#include <libbutl/timestamp.hxx>

#include <libbutl-archive/archive.hxx>

#include <iostream>

#undef NDEBUG
#include <cassert>

// Usage: driver <tar-file> <entry-file>
//
int
main (int argc, char* argv[])
{
  using namespace std;
  using namespace butl;

  assert (argc == 3);

  path af (argv[1]);
  path ef (argv[2]);

  try
  {
    archive_writer a (af);

    a.add_file_entry (ef,
                      ef,
                      0644,
                      system_clock::from_time_t (1)); // One second since epoch.

    a.close ().cancel ();
  }
  catch (const archive_error& e)
  {
    cerr << e.file.string () << ": " << e.description << endl;
    return 1;
  }
}
