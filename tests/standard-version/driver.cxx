// file      : tests/standard-version/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <ios>       // ios::failbit, ios::badbit
#include <string>
#include <cassert>
#include <iostream>
#include <stdexcept> // invalid_argument

#include <butl/utility>          // operator<<(ostream,exception)
#include <butl/standard-version>

using namespace std;
using namespace butl;

// Create standard version from string, and also test another ctors.
//
static standard_version
version (const string& s, bool allow_earliest = true)
{
  standard_version r (s, allow_earliest);

  try
  {
    standard_version v (r.epoch,
                        r.version,
                        r.snapshot ()
                        ? r.string_snapshot ()
                        : string (),
                        r.revision,
                        allow_earliest);

    assert (r == v);

    if (r.epoch == 0 && r.revision == 0)
    {
      standard_version v (r.version,
                          r.snapshot ()
                          ? r.string_snapshot ()
                          : string (),
                          allow_earliest);
      assert (r == v);

      if (!r.snapshot ())
      {
        standard_version v (r.version, allow_earliest);
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
                          allow_earliest);
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
    cout << version (s) << endl;

  return 0;
}
catch (const invalid_argument& e)
{
  cerr << e << endl;
  return 1;
}
