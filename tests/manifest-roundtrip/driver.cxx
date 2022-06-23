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

// Usage: argv[0] [-m]
//
// Round-trip a manifest reading it from stdin and printing to stdout.
//
// -m
//    Serialize multi-line manifest values using the v2 form.
//
// -s
//    Split values into the value/comment pairs and merge them back before
//    printing.
//
int
main (int argc, const char* argv[])
try
{
  bool multiline_v2 (false);
  bool split (false);

  for (int i (1); i != argc; ++i)
  {
    string v (argv[i]);

    if (v == "-m")
      multiline_v2 = true;
    else if (v == "-s")
      split = true;
  }

  // Read/write in binary mode.
  //
  stdin_fdmode  (fdstream_mode::binary);
  stdout_fdmode (fdstream_mode::binary);

  manifest_parser p (cin, "stdin");

  manifest_serializer s (cout,
                         "stdout",
                         false /* long_lines */,
                         {} /* filter */,
                         multiline_v2);

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

    if (split)
    {
      const auto& vc (manifest_parser::split_comment (nv.value));
      nv.value = manifest_serializer::merge_comment (vc.first, vc.second);
    }

    s.next (nv.name, nv.value);
  }
}
catch (const exception& e)
{
  cerr << e << endl;
  return 1;
}
