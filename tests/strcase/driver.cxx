// file      : tests/strcase/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <cassert>

#include <butl/utility>

using namespace std;
using namespace butl;

int
main ()
{
  const string upper ("+/0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  const string lower ("+/0123456789abcdefghijklmnopqrstuvwxyz");

  assert (casecmp (upper, lower) == 0);
  assert (casecmp (upper, lower, upper.size ()) == 0);
  assert (casecmp (upper, lower, 100) == 0);
  assert (casecmp ("a", "A1") < 0);
  assert (casecmp ("A1", "a") > 0);
  assert (casecmp ("a", "A1", 1) == 0);
  assert (casecmp ("A1", "a", 1) == 0);
  assert (casecmp ("a", "b", 0) == 0);

  for (size_t i (0); i < upper.size (); ++i)
  {
    assert (casecmp (upper[i], lower[i]) == 0);

    if (i > 0)
    {
      assert (casecmp (upper[i], lower[i - 1]) > 0);
      assert (casecmp (lower[i - 1], upper[i]) < 0);
    }
  }

  // As casecmp() compares strings as if they have been converted to the
  // lower case the characters [\]^_` (located between 'Z' and 'a' in the ASCII
  // table) evaluates as less than any alphabetic character.
  //
  string ascii_91_96 ("[\\]^_`");
  for (const auto& c: ascii_91_96)
  {
    assert (casecmp (&c, "A", 1) < 0);
    assert (casecmp (&c, "a", 1) < 0);
  }

  assert (ucase (lower) == upper);
  assert (lcase (upper) == lower);

  assert (ucase (lower.c_str (), 20) == string (upper, 0, 20));
  assert (lcase (upper.c_str (), 20) == string (lower, 0, 20));

  assert (ucase (lower.c_str (), 0) == string ());
  assert (lcase (upper.c_str (), 0) == string ());

  assert (ucase ("") == string ());
  assert (lcase ("") == string ());

  string s (upper);
  assert (lcase (s) == lower);

  s = lower;
  ucase (const_cast<char*> (s.data ()), s.size ());
  assert (s == upper);
}
