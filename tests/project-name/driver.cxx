// file      : tests/project-name/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <ios>       // ios::*bit
#include <string>
#include <iostream>
#include <stdexcept> // invalid_argument

#include <libbutl/utility.hxx>      // operator<<(ostream,exception), eof(),
                                    // *case()
#include <libbutl/project-name.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Create project_name from string and also perform some tests for the created
// object.
//
static project_name
name (const string& s)
{
  project_name r (s);

  assert (r == project_name (lcase (s)));
  assert (r == project_name (ucase (s)));

  assert (r > project_name ("!", project_name::raw_string));
  assert (r < project_name ("~", project_name::raw_string));

  return r;
}

// Usage: argv[0] (string|base [ext]|extension|variable)
//
// Create project names from stdin lines, and for each of them print the
// result of the specified member function to stdout, one per line.
//
int
main (int argc, char* argv[])
try
{
  assert (argc <= 3);

  string m (argv[1]);
  assert (m == "string" || m == "base" || m == "extension" || m == "variable");
  assert (m == "base" ? argc <= 3 : argc == 2);

  cin.exceptions  (ios::badbit);
  cout.exceptions (ios::failbit | ios::badbit);

  const char* ext (argc == 3 ? argv[2] : nullptr);

  string l;
  while (!eof (getline (cin, l)))
  {
    project_name n (name (l));

    const string& s (m == "string"    ? n.string ()    :
                     m == "base"      ? n.base (ext)   :
                     m == "extension" ? n.extension () :
                     n.variable ());

    cout << s << endl;
  }

  return 0;
}
catch (const invalid_argument& e)
{
  cerr << e << endl;
  return 1;
}
