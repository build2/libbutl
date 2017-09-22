// file      : tests/standard-version/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules
#include <ios>       // ios::failbit, ios::badbit
#include <string>
#include <iostream>
#include <stdexcept> // invalid_argument
#endif

// Other includes.

#ifdef __cpp_modules
#ifdef __cpp_lib_modules
import std.core;
import std.io;
#endif
import butl.utility;          // operator<<(ostream,exception)
import butl.standard_version;
#else
#include <libbutl/utility.mxx>
#include <libbutl/standard-version.mxx>
#endif

using namespace std;
using namespace butl;

// Create standard version from string, and also test another ctors.
//
static standard_version
version (const string& s,
         standard_version::flags f =
         standard_version::allow_earliest | standard_version::allow_stub)
{
  standard_version r (s, f);

  try
  {
    standard_version v (r.epoch,
                        r.version,
                        r.snapshot ()
                        ? r.string_snapshot ()
                        : string (),
                        r.revision,
                        f);

    assert (r == v);

    if (r.epoch == 0 && r.revision == 0)
    {
      standard_version v (r.version,
                          r.snapshot ()
                          ? r.string_snapshot ()
                          : string (),
                          f);
      assert (r == v);

      if (!r.snapshot ())
      {
        standard_version v (r.version, f);
        assert (r == v);
      }
    }

    if (r.snapshot ())
    {
      standard_version v (r.epoch,
                          r.version,
                          r.snapshot_sn,
                          r.snapshot_id,
                          r.revision,
                          f);
      assert (r == v);
    }

  }
  catch (const invalid_argument& e)
  {
    cerr << e << endl;
    assert (false);
  }

  return r;
}

// Usages:
//
// argv[0] -a <version>
// argv[0] -b <version>
// argv[0] -c <version> <version>
// argv[0] -r
// argv[0] -s <version> <constraint>
// argv[0]
//
// -a  output 'y' for alpha-version, 'n' otherwise
// -b  output 'y' for beta-version, 'n' otherwise
// -c  output 0 if versions are equal, -1 if the first one is less, 1 otherwise
// -r  create version constraints from STDIN lines, and print them to STDOUT
// -s  output 'y' if version satisfies constraint, 'n' otherwise
//
// If no options are specified, then create versions from STDIN lines, and
// print them to STDOUT.
//
int
main (int argc, char* argv[])
try
{
  cin.exceptions  (ios::badbit);
  cout.exceptions (ios::failbit | ios::badbit);

  if (argc > 1)
  {
    string o (argv[1]);

    if (o == "-a")
    {
      assert (argc == 3);
      char r (version (argv[2]).alpha () ? 'y' : 'n');

      cout << r << endl;
    }
    else if (o == "-b")
    {
      assert (argc == 3);
      char r (version (argv[2]).beta () ? 'y' : 'n');

      cout << r << endl;
    }
    else if (o == "-c")
    {
      assert (argc == 4);

      int r (version (argv[2]).compare (version (argv[3])));
      cout << r << endl;
    }
    else if (o == "-r")
    {
      assert (argc == 2);

      string s;
      while (getline (cin, s))
        cout << standard_version_constraint (s) << endl;
    }
    else if (o == "-s")
    {
      assert (argc == 4);

      char r (standard_version_constraint (argv[3]).satisfies (
                version (argv[2])) ? 'y' : 'n');

      cout << r << endl;
    }
    else
      assert (false);

    return 0;
  }

  string s;
  while (getline (cin, s))
    cout << (s.empty () ? standard_version () : version (s)) << endl;

  return 0;
}
catch (const invalid_argument& e)
{
  cerr << e << endl;
  return 1;
}
