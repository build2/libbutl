// file      : tests/openssl/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <vector>
#include <iostream>
#include <iterator>
#include <system_error>

#include <libbutl/path.hxx>
#include <libbutl/utility.hxx>  // operator<<(ostream, exception)
#include <libbutl/openssl.hxx>
#include <libbutl/fdstream.hxx> // nullfd

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Usage: argv[0]
//
int
main (int, const char* argv[])
try
{
  using butl::optional;

  // Test openssl rand command.
  //
  {
    openssl os (nullfd, path ("-"), 2, path ("openssl"), "rand", 128);

    vector<char> r (os.in.read_binary ());
    os.in.close ();

    assert (os.wait () && r.size () == 128);
  }

  // Test openssl info retrieval.
  //
  {
    optional<openssl_info> v (openssl::info (2, path ("openssl")));

    assert (v);
  }

  return 0;
}
catch (const system_error& e)
{
  cerr << argv[0] << ": " << e << endl;
  return 1;
}
