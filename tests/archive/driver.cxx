// file      : tests/archive/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl-archive/archive.hxx>

#undef NDEBUG
#include <cassert>

// Usage: driver <tar-file> <entry-file>
//
int
main (int argc, char* argv[])
{
  using namespace butl;

  assert (argc == 3);

  archive (argv[1],
           argv[2],
           system_clock::from_time_t (1)); // One second since epoch.
}
