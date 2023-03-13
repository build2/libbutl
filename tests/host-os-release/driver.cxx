// file      : tests/host-os-release/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/host-os-release.hxx>

#include <libbutl/path.hxx>

namespace butl
{
  LIBBUTL_SYMEXPORT os_release
  host_os_release_linux (path f = {});
}

#include <iostream>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

int
main (int argc, char* argv[])
{
  assert (argc >= 2); // <host-target-triplet>

  target_triplet host (argv[1]);

  os_release r;
  if (host.class_ == "linux")
  {
    assert (argc == 3); // <host-target-triplet> <file-path>
    r = host_os_release_linux (path (argv[2]));
  }
  else
  {
    assert (argc == 2);
    if (optional<os_release> o = host_os_release (host))
      r = move (*o);
    else
    {
      cerr << "unrecognized host os " << host.string () << endl;
      return 1;
    }
  }

  cout << r.name_id << '\n';
  for (auto b (r.like_ids.begin ()), i (b); i != r.like_ids.end (); ++i)
    cout << (i != b ? "|" : "") << *i;
  cout << '\n'
       << r.version_id << '\n'
       << r.variant_id << '\n'
       << r.name << '\n'
       << r.version_codename << '\n'
       << r.variant << '\n';

  return 0;
}
