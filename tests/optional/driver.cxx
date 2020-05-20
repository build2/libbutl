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

struct redirect
{
  redirect () = default;

  redirect (redirect&&) = default;
  redirect& operator= (redirect&&) = default;

  redirect (const redirect&) = delete;
  redirect& operator= (const redirect&) = delete;
};

int
main ()
{
  using butl::optional;

  optional<redirect> r;
  vector<optional<redirect>> rs;
  rs.emplace_back (move (r));
}
