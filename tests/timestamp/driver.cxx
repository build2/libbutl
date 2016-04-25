// file      : tests/timestamp/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <time.h> // tzset()

#include <chrono>
#include <locale>
#include <clocale>
#include <cassert>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <system_error>

#include <butl/timestamp>

using namespace std;
using namespace butl;

// Parse the input using the format string. Print the resulted time with the
// same format string, ensure the output matches the input.
//
static bool
parse (const char* in, const char* fmt, bool local, string out)
{
  if (out.empty ())
    out = in;

  try
  {
    const char* e;
    timestamp t (from_string (in, fmt, local, &e));

    ostringstream o;
    if (!to_stream (o, t, fmt, false, local))
      return false;

    return o.str () + e == out;
  }
  catch (...)
  {
    return false;
  }
}

static bool
parse (const char* in, const char* fmt, const string& out = "")
{
  return parse (in, fmt, true, out) && parse (in, fmt, false, out);
}

static bool
fail (const char* in, const char* fmt)
{
  try
  {
    from_string (in, fmt, true);
    return false;
  }
  catch (const system_error&)
  {
    return true;
  }
}

// Convert nanoseconds to string according to the butl::duration period.
//
static string
ns (unsigned long long t)
{
  duration d (chrono::duration_cast<duration> (chrono::nanoseconds (t)));
  chrono::nanoseconds n (chrono::duration_cast<chrono::nanoseconds> (d));

  ostringstream o;
  o.fill ('0');
  o << setw (9) << n.count ();
  return o.str ();
}

int
main ()
{
  tzset (); // To use butl::to_stream() later on.

  // Invalid %[].
  //
  assert (fail ("Apr 08 19:31:10 2016", "%b %d %H:%M:%S%["));
  assert (fail ("Apr 08 19:31:10 2016", "%b %d %H:%M:%S%[."));
  assert (fail ("Apr 08 19:31:10 2016", "%b %d %H:%M:%S%[.U"));
  assert (fail ("Apr 08 19:31:10 2016", "%b %d %H:%M:%S%[.A]"));
  assert (fail ("Apr 08 19:31:10 2016", "%d %H:%M:%S%[.U] %Y"));
  assert (fail ("2016-10-20 11:12:13.123456789", "%Y-%m-%d %H:%M:%S%[N]"));

  // Invalid fraction of a second.
  //
  assert (fail ("Apr 08 19:31:10. 2016", "%b %d %H:%M:%S%[.U] %Y"));
  assert (fail ("Apr 08 19:31:10.1 2016", "%b %d %H:%M:%S%[.M] %Y"));
  assert (fail ("Apr 08 19:31:10.12 2016", "%b %d %H:%M:%S%[.M] %Y"));
  assert (fail ("Apr 08 19:31:10.", "%b %d %H:%M:%S%[.U] %Y"));
  assert (fail ("Apr 08 19:31:10.1", "%b %d %H:%M:%S%[.M] %Y"));
  assert (fail ("Apr 08 19:31:10.12", "%b %d %H:%M:%S%[.M] %Y"));

  // Input is not fully parsed.
  //
  assert (fail (
    "Feb 21 19:31:10.123456789 2016 GMT", "%b %d %H:%M:%S%[.N] %Y"));

  // Invalid input (%[] unrelated).
  //
  assert (fail ("Apr 08 19:31:10.123456789 ABC", "%b %d %H:%M:%S%[.N] %Y"));
  assert (fail ("Apr 19:31:10 2016", "%b %d %H:%M:%S %Y"));
  assert (fail ("Opr 08 19:31:10 2016", "%b %d %H:%M:%S %Y"));

  // Parse valid input with a valid format.
  //
  assert (parse (
    "Apr  18 19:31:10 2016", "%b %d %H:%M:%S  %Y", "Apr 18 19:31:10  2016"));

  assert (parse ("Apr 08 19:31:10 2016", "%b %d %H:%M:%S %Y"));
  assert (parse ("2016-04-08 19:31:10", "%Y-%m-%d %H:%M:%S"));

  assert (parse ("ABC=Apr 18 19:31:10 2016 ABC", "ABC=%b %d %H:%M:%S %Y"));
  assert (parse ("ABC=2016-04-08 19:31:10 ABC", "ABC=%Y-%m-%d %H:%M:%S"));

  assert (parse ("Feb 11 19:31:10 2016 GMT", "%b %d %H:%M:%S%[.N] %Y"));
  assert (parse ("2016-02-11 19:31:10 GMT", "%Y-%m-%d %H:%M:%S%[.N]"));

  assert (parse ("Feb 21 19:31:10.384902285 2016 GMT",
                 "%b %d %H:%M:%S%[.N] %Y",
                 "Feb 21 19:31:10." + ns (384902285) + " 2016 GMT"));

  assert (parse ("2016-02-21 19:31:10.384902285 GMT",
                 "%Y-%m-%d %H:%M:%S%[.N]",
                 "2016-02-21 19:31:10." + ns (384902285) + " GMT"));

  assert (parse ("Feb 21 19:31:10 .384902285 2016 GMT",
                 "%b %d %H:%M:%S %[.N] %Y",
                 "Feb 21 19:31:10 ." + ns (384902285) + " 2016 GMT"));

  assert (parse ("2016-02-21 19:31:10 .384902285 GMT",
                 "%Y-%m-%d %H:%M:%S %[.N]",
                 "2016-02-21 19:31:10 ." + ns (384902285) + " GMT"));

  assert (parse (
    "2016-02-21 19:31:10  .384902285 GMT",
    "%Y-%m-%d %H:%M:%S %[.N]",
    "2016-02-21 19:31:10 ." + ns (384902285) + " GMT"));

  assert (parse (
    "2016-02-21 19:31:10 .384902285  GMT",
    "%Y-%m-%d %H:%M:%S  %[.N]",
    "2016-02-21 19:31:10  ." + ns (384902285) + "  GMT"));

  assert (parse ("Feb 21 19:31:10 .384902285NS 2016 GMT",
                 "%b %d %H:%M:%S %[.N]NS %Y",
                 "Feb 21 19:31:10 ." + ns (384902285) + "NS 2016 GMT"));

  assert (parse ("2016-02-21 19:31:10 .384902285NS GMT",
                 "%Y-%m-%d %H:%M:%S %[.N]NS",
                 "2016-02-21 19:31:10 ." + ns (384902285) + "NS GMT"));

  assert (parse (".384902285 Feb 21 19:31:10 2016",
                 "%[.N] %b %d %H:%M:%S %Y",
                 "." + ns (384902285) + " Feb 21 19:31:10 2016"));

  assert (parse (".384902285 2016-02-21 19:31:10",
                 "%[.N] %Y-%m-%d %H:%M:%S",
                 "." + ns (384902285) + " 2016-02-21 19:31:10"));

  assert (parse (".3849022852016-02-21 19:31:10",
                 "%[.N]%Y-%m-%d %H:%M:%S",
                 "." + ns (384902285) + "2016-02-21 19:31:10"));

  /*
  setlocale (LC_ALL, "de_DE.utf-8");
  locale::global (locale ("de_DE.utf-8"));
  assert (parse ("Mai 11 19:31:10 2016 GMT", "%b %d %H:%M:%S%[.N] %Y"));
  locale::global (locale ("C"));
  */

  // @@ When debuging strptime() fallback implementation compiled with GCC
  //    5.3.1, the following asserts will fail due to bugs in implementation
  //    of std::get_time() manipulator. So need to be commented out.
  //
  assert (fail ("Apr 08 19:31:10 2016", "%b %d %H:%M:%S %Y %"));
  assert (fail ("Apr 08 19:31:10", "%b %d %H:%M:%S %Y"));

  assert (parse (
    "Apr  8 19:31:10 2016", "%b %d %H:%M:%S %Y", "Apr 08 19:31:10 2016"));
}
