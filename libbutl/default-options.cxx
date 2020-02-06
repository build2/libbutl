// file      : libbutl/default-options.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules_ts
#include <libbutl/default-options.mxx>
#endif

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <vector>
#endif

// Other includes.

#ifdef __cpp_modules_ts
module butl.default_options;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules_ts
import std.core;
#endif
import butl.path;
import butl.optional;
import butl.small_vector;
#endif

#endif

using namespace std;

namespace butl
{
  optional<dir_path>
  default_options_start (const optional<dir_path>& home,
                         const vector<dir_path>& dirs)
  {
    if (home)
      assert (home->absolute () && home->normalized ());

    if (dirs.empty ())
      return nullopt;

    // Use the first directory as a start.
    //
    auto i (dirs.begin ());
    dir_path d (*i);

    // Try to find a common prefix for each subsequent directory.
    //
    for (++i; i != dirs.end (); ++i)
    {
      bool p (false);

      for (;
           !(d.root () || (home && d == *home));
           d = d.directory ())
      {
        if (i->sub (d))
        {
          p = true;
          break;
        }
      }

      if (!p)
        return nullopt;
    }

    return d;
  }
}
