// file      : tests/lz4/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <iostream>
#include <exception>

#include <libbutl/lz4.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/filesystem.hxx> // entry_stat, path_entry()

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Usage: argv[0] [-c|-d] <input-file> <output-file>
//
int
main (int argc, const char* argv[])
try
{
  assert (argc == 4);

  ifdstream ifs (argv[2], fdopen_mode::binary, ifdstream::badbit);
  ofdstream ofs (argv[3], fdopen_mode::binary);

  if (argv[1][1] == 'c')
  {
    lz4::compress (ofs, ifs,
                   1 /* compression_level */,
                   4 /* block_size_id (64KB) */,
                   fdstat (ifs.fd ()).size);
  }
  else
  {
    lz4::decompress (ofs, ifs);
  }

  ofs.close ();
}
catch (const std::exception& e)
{
  cerr << e.what () << endl;
  return 1;
}
