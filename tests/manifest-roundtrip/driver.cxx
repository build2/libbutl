// file      : tests/manifest-roundtrip/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules
#include <string>
#include <iostream>
#endif

// Other includes.

#ifdef __cpp_modules
#ifdef __cpp_lib_modules
import std.core;
import std.io;
#endif
import butl.utility;             // operator<<(ostream, exception)
import butl.fdstream;
import butl.manifest_parser;
import butl.manifest_serializer;
#else
#include <libbutl/utility.mxx>
#include <libbutl/fdstream.mxx>
#include <libbutl/manifest-parser.mxx>
#include <libbutl/manifest-serializer.mxx>
#endif

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
    cerr << e << endl;
    return 1;
  }
}
