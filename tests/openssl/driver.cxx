// file      : tests/openssl/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <vector>
#include <iostream>
#include <iterator>
#include <system_error>

#include <libbutl/path.hxx>
#include <libbutl/utility.hxx> // operator<<(ostream, exception)
#include <libbutl/openssl.hxx>

using namespace std;
using namespace butl;

// Usage: argv[0]
//
int
main (int, const char* argv[])
try
{
  openssl os (nullfd, path ("-"), 2, path ("openssl"), "rand", 128);

  vector<char> r (os.in.read_binary ());
  os.in.close ();

  return os.wait () && r.size () == 128 ? 0 : 1;
}
catch (const system_error& e)
{
  cerr << argv[0] << ": " << e << endl;
  return 1;
}
