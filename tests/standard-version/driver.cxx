// file      : tests/standard-version/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <ios>       // ios::failbit, ios::badbit
#include <string>
#include <cstdint>   // uint*_t
#include <iostream>
#include <stdexcept> // invalid_argument

#include <libbutl/utility.hxx>          // operator<<(ostream,exception), eof()
#include <libbutl/optional.hxx>
#include <libbutl/standard-version.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Return the standard version created from a string, and also perform some
// resulting version tests (check other constructors, invariants, etc.).
//
static standard_version
version (const string& s,
         standard_version::flags f =
         standard_version::allow_earliest | standard_version::allow_stub)
{
  standard_version r (s, f);

  // Test the other constructors.
  //
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

    if (r.epoch == 1 && r.revision == 0)
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

    // Test using the resulting version with shortcut operators.
    //
    if (!r.stub ())
    {
      auto max_ver = [&v] (char c) -> string
      {
        string e (v.epoch != 1 ? '+' + to_string (v.epoch) + '-' : string ());

        return c == '~' || v.major () == 0
        ? e + to_string (v.major ()) + '.' + to_string (v.minor () + 1) + ".0-"
        : e + to_string (v.major () + 1) + ".0.0-";
      };

      if (v.minor () != 99999)
      {
        standard_version_constraint c1 ('~' + s);
        standard_version_constraint c2 ('[' + s + ' ' + max_ver ('~') + ')');
        assert (c1 == c2);
      }

      if ((v.major () == 0 && v.minor () != 99999) ||
          (v.major () != 0 && v.major () != 99999))
      {
        standard_version_constraint c1 ('^' + s);
        standard_version_constraint c2 ('[' + s + ' ' + max_ver ('^') + ')');
        assert (c1 == c2);
      }
    }

    // Check some invariants for the resulting version.
    //
    // Stub is not a final (pre-)release nor snapshot.
    //
    assert (!r.stub () || !(r.final () || r.snapshot ()));

    // Earliest is a final alpha.
    //
    assert (!r.earliest () || (r.final () && r.alpha ()));

    // Final is a release or a pre-release but not a snapshot.
    //
    assert (r.final () ==
            (r.release () || (r.pre_release () && !r.snapshot ())));

    // Snapshot is a pre-release.
    //
    assert (!r.snapshot () || r.pre_release ());

    // Pre-release is either alpha or beta.
    //
    assert (r.pre_release ().has_value () == (r.alpha () || r.beta ()));
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
// argv[0] (-rl|-pr|-al|-bt|-st|-el|-sn|-fn) <version>
// argv[0] -cm <version> <version>
// argv[0] -cr [<dependent-version>]
// argv[0] -sf <version> <constraint>
// argv[0]
//
// -rl  output 'y' for release, 'n' otherwise
// -pr  output DDD version part for pre-release, '-' otherwise
// -al  output alpha version number for alpha-version, '-' otherwise
// -bt  output beta version number for beta-version, '-' otherwise
// -st  output 'y' for stub, 'n' otherwise
// -el  output 'y' for earliest, 'n' otherwise
// -sn  output 'y' for snapshot, 'n' otherwise
// -fn  output 'y' for final, 'n' otherwise
//
// -cm  output 0 if versions are equal, -1 if the first one is less, 1
//      otherwise
//
// -cr  create version constraints from stdin lines, optionally using the
//      dependent version, and print them to stdout
//
// -sf  output 'y' if version satisfies constraint, 'n' otherwise
//
// If no options are specified, then create versions from stdin lines, and
// print them to stdout.
//
int
main (int argc, char* argv[])
try
{
  using butl::optional;

  cin.exceptions  (ios::badbit);
  cout.exceptions (ios::failbit | ios::badbit);

  if (argc > 1)
  {
    string o (argv[1]);

    if (o == "-rl")
    {
      assert (argc == 3);
      cout << (version (argv[2]).release () ? 'y' : 'n') << endl;
    }
    else if (o == "-pr")
    {
      assert (argc == 3);

      if (optional<uint16_t> n = version (argv[2]).pre_release ())
        cout << *n << endl;
      else
        cout << '-' << endl;
    }
    else if (o == "-al")
    {
      assert (argc == 3);

      if (optional<uint16_t> n = version (argv[2]).alpha ())
        cout << *n << endl;
      else
        cout << '-' << endl;
    }
    else if (o == "-bt")
    {
      assert (argc == 3);

      if (optional<uint16_t> n = version (argv[2]).beta ())
        cout << *n << endl;
      else
        cout << '-' << endl;
    }
    else if (o == "-st")
    {
      assert (argc == 3);
      cout << (version (argv[2]).stub () ? 'y' : 'n') << endl;
    }
    else if (o == "-el")
    {
      assert (argc == 3);
      cout << (version (argv[2]).earliest () ? 'y' : 'n') << endl;
    }
    else if (o == "-sn")
    {
      assert (argc == 3);
      cout << (version (argv[2]).snapshot () ? 'y' : 'n') << endl;
    }
    else if (o == "-fn")
    {
      assert (argc == 3);
      cout << (version (argv[2]).final () ? 'y' : 'n') << endl;
    }
    else if (o == "-cm")
    {
      assert (argc == 4);

      int r (version (argv[2]).compare (version (argv[3])));
      cout << r << endl;
    }
    else if (o == "-cr")
    {
      assert (argc <= 3);

      optional<standard_version> dv;
      if (argc == 3)
      {
        string s (argv[2]);

        dv = s.empty ()
          ? standard_version ()
          : standard_version (s,
                              standard_version::allow_stub |
                              standard_version::allow_earliest);
      }

      string s;
      while (getline (cin, s))
        cout << (dv
                 ? standard_version_constraint (s, *dv)
                 : standard_version_constraint (s)) << endl;
    }
    else if (o == "-sf")
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
  while (!eof (getline (cin, s)))
    cout << (s.empty () ? standard_version () : version (s)) << endl;

  return 0;
}
catch (const invalid_argument& e)
{
  cerr << e << endl;
  return 1;
}
