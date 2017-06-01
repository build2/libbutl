// file      : tests/manifest-serializer/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <vector>
#include <string>
#include <utility> // pair
#include <cassert>
#include <sstream>
#include <iostream>

#include <libbutl/manifest-serializer.hxx>

using namespace std;
using namespace butl;

using pairs = vector<pair<string, string>>;

static bool
test (const pairs& manifest, const string& expected);

static bool
fail (const pairs& manifest);

int
main ()
{
  // Comments.
  //
  assert (test ({{"#", ""}}, "#\n"));
  assert (test ({{"#", "x"}}, "# x\n"));
  assert (test ({{"#", "x"},{"#", "y"},{"#", ""}}, "# x\n# y\n#\n"));
  assert (fail ({{"",""},{"#", "x"}})); // serialization after eos

  // Empty manifest stream.
  //
  assert (test ({}, ""));
  assert (test ({{"",""}}, ""));

  // Empty manifest.
  //
  assert (test ({{"","1"},{"",""},{"",""}}, ": 1\n"));
  assert (test ({{"","1"},{"",""},{"","1"},{"",""},{"",""}}, ": 1\n:\n"));

  // Invalid manifests.
  //
  assert (fail ({{"a",""}}));                  // format version pair expected
  assert (fail ({{"","1"},{"",""},{"a",""}})); // format version pair expected
  assert (fail ({{"","9"}}));                  // unsupported format version 9
  assert (fail ({{"","1"},{"","x"}}));         // non-empty value in end pair
  assert (fail ({{"",""},{"","1"}}));          // serialization after eos

  // Single manifest.
  //
  assert (test ({{"","1"},{"a","x"},{"",""},{"",""}}, ": 1\na: x\n"));
  assert (test ({{"","1"},{"a","x"},{"b","y"},{"",""},{"",""}},
                ": 1\na: x\nb: y\n"));
  assert (test ({{"","1"},{"#","c"},{"a","x"},{"",""},{"",""}},
                ": 1\n# c\na: x\n"));

  // Multiple manifests.
  //
  assert (test ({{"","1"},{"a","x"},{"",""},
                 {"","1"},{"b","y"},{"",""},{"",""}}, ": 1\na: x\n:\nb: y\n"));
  assert (test ({{"","1"},{"a","x"},{"",""},
                 {"","1"},{"b","y"},{"",""},
                 {"","1"},{"c","z"},{"",""},{"",""}},
                ": 1\na: x\n:\nb: y\n:\nc: z\n"));

  // Invalid name.
  //
  assert (fail ({{"","1"},{"#a",""}}));
  assert (fail ({{"","1"},{"a:b",""}}));
  assert (fail ({{"","1"},{"a b",""}}));
  assert (fail ({{"","1"},{"a\tb",""}}));
  assert (fail ({{"","1"},{"a\n",""}}));

  // Simple value.
  //
  assert (test ({{"","1"},{"a",""},{"",""},{"",""}}, ": 1\na:\n"));
  assert (test ({{"","1"},{"a","x y z"},{"",""},{"",""}}, ": 1\na: x y z\n"));

  // Long simple value (newline escaping).
  //

  // "Solid" text/hard break.
  //
  string l1 ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
             "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
             "Yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
             "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
             "Zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
             "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");

  string e1 ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
             "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\\\n"
             "Yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
             "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy\\\n"
             "Zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
             "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");

  // Space too early/hard break.
  //
  string l2 ("x xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
             "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
             "Yyyyyyyyyyyyyyyyy yyyyyyyyyyyyyyyyyyy"
             "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
             "Zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz z"
             "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");

  string e2 ("x xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
             "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\\\n"
             "Yyyyyyyyyyyyyyyyy yyyyyyyyyyyyyyyyyyy"
             "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy\\\n"
             "Zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz z"
             "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");

  // Space/soft break.
  //
  string l3 ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
             "xxxxxxxxxxxxxxxxxxx"
             " Yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
             "yyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
             " Zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
             "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");

  string e3 ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
             "xxxxxxxxxxxxxxxxxxx\\\n"
             " Yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
             "yyyyyyyyyyyyyyyyyyyyyyyyyyyyy\\\n"
             " Zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
             "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");

  // Space with a better one/soft break.
  //
  string l4 ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
             "xxxxxxxxx xxxxxxxxx"
             " Yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
             "yyyyyyyyyyyyyyyyyy yyyyyyyyyy"
             " Zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
             "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");

  string e4 ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
             "xxxxxxxxx xxxxxxxxx\\\n"
             " Yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
             "yyyyyyyyyyyyyyyyyy yyyyyyyyyy\\\n"
             " Zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
             "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");

  // Hard break after the backslash/delayed hard break.
  //
  string l5 ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
             "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\\"
             "Yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy");

  string e5 ("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
             "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\\Y\\\n"
             "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy");

  assert (test ({{"","1"},{"a",l1},{"",""},{"",""}}, ": 1\na: " + e1 + "\n"));
  assert (test ({{"","1"},{"a",l2},{"",""},{"",""}}, ": 1\na: " + e2 + "\n"));
  assert (test ({{"","1"},{"a",l3},{"",""},{"",""}}, ": 1\na: " + e3 + "\n"));
  assert (test ({{"","1"},{"a",l4},{"",""},{"",""}}, ": 1\na: " + e4 + "\n"));
  assert (test ({{"","1"},{"a",l5},{"",""},{"",""}}, ": 1\na: " + e5 + "\n"));


  // Multi-line value.
  //
  string n ("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  assert (test ({{"","1"},{n,"x"},{"",""},{"",""}},
                ": 1\n" + n + ": \\\nx\n\\\n"));
  assert (test ({{"","1"},{"a","\n"},{"",""},{"",""}},
                ": 1\na: \\\n\n\n\\\n"));
  assert (test ({{"","1"},{"a","\n\n"},{"",""},{"",""}},
                ": 1\na: \\\n\n\n\n\\\n"));
  assert (test ({{"","1"},{"a","\nx\n"},{"",""},{"",""}},
                ": 1\na: \\\n\nx\n\n\\\n"));
  assert (test ({{"","1"},{"a","x\ny\nz"},{"",""},{"",""}},
                ": 1\na: \\\nx\ny\nz\n\\\n"));
  assert (test ({{"","1"},{"a"," x"},{"",""},{"",""}},
                ": 1\na: \\\n x\n\\\n"));
  assert (test ({{"","1"},{"a","x "},{"",""},{"",""}},
                ": 1\na: \\\nx \n\\\n"));
  assert (test ({{"","1"},{"a"," x "},{"",""},{"",""}},
                ": 1\na: \\\n x \n\\\n"));

  // Carriage return character.
  //
  assert (test ({{"","1"},{"a","x\ry"},{"",""},{"",""}},
                ": 1\na: \\\nx\ny\n\\\n"));
  assert (test ({{"","1"},{"a","x\r"},{"",""},{"",""}},
                ": 1\na: \\\nx\n\n\\\n"));
  assert (test ({{"","1"},{"a","x\r\ny"},{"",""},{"",""}},
                ": 1\na: \\\nx\ny\n\\\n"));
  assert (test ({{"","1"},{"a","x\r\n"},{"",""},{"",""}},
                ": 1\na: \\\nx\n\n\\\n"));

  // Extra three x's are for the leading name part ("a: ") that we
  // don't have.
  //
  assert (test ({{"","1"},{"a","\nxxx" + l1},{"",""},{"",""}},
                ": 1\na: \\\n\nxxx" + e1 + "\n\\\n"));
  assert (test ({{"","1"},{"a","\nxxx" + l2},{"",""},{"",""}},
                ": 1\na: \\\n\nxxx" + e2 + "\n\\\n"));
  assert (test ({{"","1"},{"a","\nxxx" + l3},{"",""},{"",""}},
                ": 1\na: \\\n\nxxx" + e3 + "\n\\\n"));
  assert (test ({{"","1"},{"a","\nxxx" + l4},{"",""},{"",""}},
                ": 1\na: \\\n\nxxx" + e4 + "\n\\\n"));

  // Backslash escaping (simple and multi-line).
  //
  assert (test ({{"","1"},{"a","c:\\"},{"",""},{"",""}},
                ": 1\na: c:\\\\\n"));
  assert (test ({{"","1"},{"a","c:\\\nd:\\"},{"",""},{"",""}},
                ": 1\na: \\\nc:\\\\\nd:\\\\\n\\\n"));
}

static string
serialize (const pairs& m)
{
  ostringstream os;
  os.exceptions (istream::failbit | istream::badbit);
  manifest_serializer s (os, "");

  for (const auto& p: m)
  {
    if (p.first != "#")
      s.next (p.first, p.second);
    else
      s.comment (p.second);
  }

  return os.str ();
}

static bool
test (const pairs& m, const string& e)
{
  string r (serialize (m));

  if (r != e)
  {
    cerr << "actual:" << endl << "'" << r << "'"<< endl
         << "expect:" << endl << "'" << e << "'"<< endl;

    return false;
  }

  return true;
}

static bool
fail (const pairs& m)
{
  try
  {
    string r (serialize (m));
    cerr << "nofail: " << r << endl;
    return false;
  }
  catch (const manifest_serialization&)
  {
    //cerr << e << endl;
  }

  return true;
}
