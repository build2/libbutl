// file      : tests/next-word/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <vector>
#include <string>
//#include <iostream>

#include <libbutl/utility.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

using strings = vector<string>;

static strings
parse_lines (const string& s)
{
  strings r;
  for (size_t b (0), e (0), m (0), n (s.size ());
       next_word (s, n, b, e, m, '\n', '\r'), b != n; )
  {
    //cerr << "'" << string (s, b, e - b) << "'" << endl;
    r.push_back (string (s, b, e - b));
  }
  return r;
}

int
main ()
{
  assert ((parse_lines("") == strings {}));
  assert ((parse_lines("a") == strings {"a"}));
  assert ((parse_lines("\n") == strings {"", ""}));
  assert ((parse_lines("\n\n") == strings {"", "", ""}));
  assert ((parse_lines("\n\n\n") == strings {"", "", "", ""}));
  assert ((parse_lines("\na") == strings {"", "a"}));
  assert ((parse_lines("\n\na") == strings {"", "", "a"}));
  assert ((parse_lines("a\n") == strings {"a", ""}));
  assert ((parse_lines("a\n\n") == strings {"a", "", ""}));
  assert ((parse_lines("a\nb") == strings {"a", "b"}));
  assert ((parse_lines("a\n\nb") == strings {"a", "", "b"}));
  assert ((parse_lines("\na\nb\n") == strings {"", "a", "b", ""}));
}
