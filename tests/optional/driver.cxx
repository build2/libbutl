// file      : tests/optional/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <vector>
#include <utility>    // move()
#endif

// Other includes.

#ifdef __cpp_modules_ts
#ifdef __cpp_lib_modules_ts
import std.core;
#endif
import butl.optional;
#else
#include <libbutl/optional.mxx>
#endif

using namespace std;
using namespace butl;

struct redirect
{
  redirect () = default;

  redirect (redirect&&) = default;
  redirect& operator= (redirect&&) = default;

  redirect (const redirect&) = delete;
  redirect& operator= (const redirect&) = delete;
};

struct command
{
  butl::optional<redirect> err;
};

int
main ()
{
  /*(
  command c;
  vector<command> cs;
  cs.emplace_back (move (c));
//  cs.push_back (move (c));
*/

  redirect r;
  vector<redirect> rs;
  rs.emplace_back (move (r));
}
