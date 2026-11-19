// file      : tests/archive/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/path.hxx>
#include <libbutl/timestamp.hxx>

#include <libbutl-archive/archive.hxx>

#include <iostream>

#undef NDEBUG
#include <cassert>

// Usage: driver <archive-file> <entry-archive-path> <entry-filesystem-path>
//
int
main (int argc, char* argv[])
{
  using namespace std;
  using namespace butl;

  assert (argc == 4);

  path ap (argv[1]);
  path eap (argv[2]);
  path efp (argv[3]);

  try
  {
    archive_writer a (ap);

    a.add_file_entry (efp,
                      eap,
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
