// file      : tests/manifest-roundtrip/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <iostream>

#include <libbutl/utility.hxx>             // operator<<(ostream, exception)
#include <libbutl/fdstream.hxx>
#include <libbutl/manifest-parser.hxx>
#include <libbutl/manifest-serializer.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

int
main ()
try
{
  // Read/write in binary mode.
  //
  stdin_fdmode  (fdstream_mode::binary);
  stdout_fdmode (fdstream_mode::binary);

  manifest_parser p (cin, "stdin");
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
