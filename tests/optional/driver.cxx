// file      : tests/optional/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <vector>
#include <utility>    // move()

#include <libbutl/optional.hxx>

#undef NDEBUG
#include <cassert>

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
