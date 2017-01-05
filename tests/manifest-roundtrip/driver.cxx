// file      : tests/manifest-roundtrip/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>
#include <iostream>

#include <butl/fdstream>
#include <butl/manifest-parser>
#include <butl/manifest-serializer>

using namespace std;
using namespace butl;

int
main (int argc, char* argv[])
{
  if (argc != 2)
  {
    cerr << "usage: " << argv[0] << " <file>" << endl;
    return 1;
  }

  try
  {
    ifdstream ifs (argv[1]);
    manifest_parser p (ifs, argv[1]);

    stdout_fdmode (fdstream_mode::binary); // Write in binary mode.
    manifest_serializer s (cout, "stdout");

    for (bool eom (true), eos (false); !eos; )
    {
      manifest_name_value nv (p.next ());

      if (nv.empty ()) // End pair.
      {
        eos = eom;
        eom = true;
      }
      else
        eom = false;

      s.next (nv.name, nv.value);
    }
  }
  catch (const exception& e)
  {
    cerr << e.what () << endl;
    return 1;
  }
}
