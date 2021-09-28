// file      : tests/sendmail/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <iostream>
#include <system_error>

#include <libbutl/path.hxx>
#include <libbutl/process.hxx>
#include <libbutl/utility.hxx>  // operator<<(ostream, exception)
#include <libbutl/sendmail.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

// Usage: argv[0] <to>
//
int
main (int argc, const char* argv[])
try
{
  assert (argc == 2);

  sendmail sm ([] (const char* c[], std::size_t n)
               {
                 process::print (cerr, c, n); cerr << endl;
               },
               2,
               "",
               "tests/sendmail/driver",
               {argv[1]});

  sm.out << cin.rdbuf ();
  sm.out.close ();

  if (!sm.wait ())
    return 1; // Assume diagnostics has been issued.
}
catch (const system_error& e)
{
  cerr << argv[0] << ": " << e << endl;
  return 1;
}
