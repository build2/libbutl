// file      : tests/b-info/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <iostream>

#include <libbutl/b.hxx>
#include <libbutl/path.hxx>
#include <libbutl/utility.hxx> // operator<<(ostream,exception)

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Usage: argv[0] [-b <path>] <project-dir>
//
// Print the build2 project information to stdout.
//
// -b <path>  the build program to be used to retrieve the project information
//
int
main (int argc, char* argv[])
try
{
  path b ("b");
  dir_path project;

  for (int i (1); i != argc; ++i)
  {
    string a (argv[i]);

    if (a == "-b")
    {
      ++i;

      assert (i != argc);
      b = path (argv[i]);
    }
    else
    {
      assert (project.empty ());
      project = dir_path (move (a));
    }
  }

  assert (!project.empty ());

  cout.exceptions (ios::failbit | ios::badbit);

  b_project_info pi (
    b_info (project,
            b_info_flags::ext_mods | b_info_flags::subprojects,
            1    /* verb */,
            {}   /* cmd_callback */,
            b,
            {}   /* search_fallback */,
            {"--no-default-options"}));

  cout << "project: "      << pi.project                        << endl
       << "version: "      << pi.version                        << endl
       << "summary: "      << pi.summary                        << endl
       << "url: "          << pi.url                            << endl
       << "src_root: "     << pi.src_root.representation ()     << endl
       << "out_root: "     << pi.out_root.representation ()     << endl
       << "amalgamation: " << pi.amalgamation.representation () << endl
       << "subprojects: ";

  for (auto b (pi.subprojects.begin ()), i (b);
       i != pi.subprojects.end ();
       ++i)
  {
    if (i != b)
      cout << ' ';

    cout << i->name << '@' << i->path.representation ();
  }
  cout << endl
       << "operations: ";

  for (auto b (pi.operations.begin ()), i (b); i != pi.operations.end (); ++i)
  {
    if (i != b)
      cout << ' ';

    cout << *i;
  }
  cout << endl
       << "meta-operations: ";

  for (auto b (pi.meta_operations.begin ()), i (b);
       i != pi.meta_operations.end ();
       ++i)
  {
    if (i != b)
      cout << ' ';

    cout << *i;
  }
  cout << endl
       << "modules: ";

  for (auto b (pi.modules.begin ()), i (b);
       i != pi.modules.end ();
       ++i)
  {
    if (i != b)
      cout << ' ';

    cout << *i;
  }
  cout << endl;

  return 0;
}
catch (const b_error& e)
{
  if (!e.normal ())
    cerr << e << endl;

  return 1;
}
