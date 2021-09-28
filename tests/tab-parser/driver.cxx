// file      : tests/tab-parser/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <iostream>

#include <libbutl/utility.hxx>    // operator<<(ostream,exception)
#include <libbutl/tab-parser.hxx>

#undef NDEBUG
#include <cassert>

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
