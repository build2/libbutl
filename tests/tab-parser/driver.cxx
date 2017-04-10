// file      : tests/tab-parser/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <ios>      // ios::failbit, ios::badbit
#include <cassert>
#include <iostream>

#include <butl/utility>    // operator<<(ostream,exception)
#include <butl/tab-parser>

using namespace std;
using namespace butl;

// Usage: argv[0] [-l]
//
// Read and parse tab-file from STDIN and print fields to STDOUT.
//
// -l  output each field on a separate line
//
int
main (int argc, char* argv[])
try
{
  assert (argc <= 2);
  bool fpl (false); // Print field per line.

  if (argc == 2)
  {
    assert (argv[1] == string ("-l"));
    fpl = true;
  }

  cin.exceptions  (ios::failbit | ios::badbit);
  cout.exceptions (ios::failbit | ios::badbit);

  tab_fields tl;
  tab_parser parser (cin, "cin");

  while (!(tl = parser.next ()).empty ())
  {
    if (!fpl)
    {
      for (auto b (tl.cbegin ()), i (b), e (tl.cend ()); i != e; ++i)
      {
        if (i != b)
          cout << ' ';

        cout << i->value;
      }

      cout << '\n';
    }
    else
    {
      for (const auto& tf: tl)
        cout << tf.value << '\n';
    }
  }

  return 0;
}
catch (const tab_parsing& e)
{
  cerr << e << endl;
  return 1;
}