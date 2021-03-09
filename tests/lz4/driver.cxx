// file      : tests/lz4/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <iostream>
#include <exception>

#include <libbutl/lz4.hxx>
#include <libbutl/fdstream.mxx>
#include <libbutl/filesystem.mxx> // entry_stat, path_entry()

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
    // @@ TODO: would be nice to get it from fd.
    //
    entry_stat st (path_entry (argv[2], true /* follow_symlinks */).second);

    lz4::compress (ofs, ifs,
                   1 /* compression_level */,
                   4 /* block_size_id (64KB) */,
                   st.size);
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
