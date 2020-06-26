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

struct move_only
{
  move_only () = default;

  move_only (move_only&&) = default;
  move_only& operator= (move_only&&) = default;

  move_only (const move_only&) = delete;
  move_only& operator= (const move_only&) = delete;
};

int
main ()
{
  using butl::optional;

  optional<move_only> r;
  vector<optional<move_only>> rs;
  rs.emplace_back (move (r));
}
