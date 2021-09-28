// file      : tests/string-parser/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <vector>
#include <iostream>

#include <libbutl/utility.hxx>       // operator<<(ostream,exception)
#include <libbutl/string-parser.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl::string_parser;

// Usage: argv[0] [-l] [-u] [-p]
//
// Read and parse lines into strings from STDIN and print them to STDOUT.
//
// -l  output each string on a separate line
// -u  unquote strings
// -p  output positions
//
int
main (int argc, char* argv[])
try
{
  bool spl (false);     // Print string per line.
  bool unquote (false);
  bool pos (false);

  for (int i (1); i != argc; ++i)
  {
    string o (argv[i]);

    if (o == "-l")
      spl = true;
    else if (o == "-u")
      unquote = true;
    else if (o == "-p")
      pos = true;
    else
      assert (false);
  }

  // Do not throw when eofbit is set (end of stream reached), and when failbit
  // is set (getline() failed to extract any character).
  //
  cin.exceptions  (ios::badbit);

  cout.exceptions (ios::failbit | ios::badbit);

  string l;
  while (getline (cin, l))
  {
    vector<pair<string, size_t>> v (parse_quoted_position (l, unquote));

    if (!spl)
    {
      for (auto b (v.cbegin ()), i (b), e (v.cend ()); i != e; ++i)
      {
        if (i != b)
          cout << ' ';

        if (pos)
          cout << i->second << ":";

        cout << i->first;
      }

      cout << endl;
    }
    else
    {
      for (const auto& s: v)
      {
        if (pos)
          cout << s.second << ":";

        cout << s.first << endl;
      }
    }
  }

  return 0;
}
catch (const invalid_string& e)
{
  cerr << e.position << ": " << e << endl;
  return 1;
}
