// file      : libbutl/target-triplet.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules_ts
#include <libbutl/target-triplet.mxx>
#endif

// C includes.

#ifndef __cpp_lib_modules_ts
#include <string>
#include <ostream>

#include <stdexcept> // invalid_argument
#endif

// Other includes.

#ifdef __cpp_modules_ts
module butl.target_triplet;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules_ts
import std.core;
import std.io;
#endif
#endif

#endif

using namespace std;

namespace butl
{
  target_triplet::
  target_triplet (const std::string& s)
  {
    using std::string;

    auto bad = [](const char* m) {throw invalid_argument (m);};

    // Find the first and the last components. The first is CPU and the last is
    // (part of) SYSTEM, that we know for sure.
    //
    string::size_type f (s.find ('-')), l (s.rfind ('-'));

    if (f == 0 || f == string::npos)
      bad ("missing cpu");

    cpu.assign (s, 0, f);

    // If we have something in between, then the first component after CPU is
    // VENDOR. Unless it is a first component of two-component system, as in
    // i686-linux-gnu.
    //
    if (f != l)
    {
      // [f, p) is VENDOR.
      //
      string::size_type p (s.find ('-', ++f)), n (p - f);

      if (n == 0) {
        goto netbsd_empty_vendor;
      }

      // Do we have all four components? If so, then we don't need to do any
      // special recognition of two-component systems.
      //
      if (l != p)
      {
        l = s.rfind ('-', --l);

        if (l != p)
          bad ("too many components");

        // Handle the none-* case here.
        //
        if (s.compare (l + 1, 5, "none-") == 0)
          l += 5;
      }
      else
      {
        // See if this is one of the well-known non-vendors.
        //
        if (s.compare (f, n, "linux") == 0   ||
            s.compare (f, n, "windows") == 0 ||
            s.compare (f, n, "kfreebsd") == 0)
        {
          l = f - 1;
          n = 0; // No VENDOR.
        }
      }

      // Handle special VENDOR values.
      //
      if (n != 0)
      {
        if (s.compare (f, n, "pc") != 0 &&
            s.compare (f, n, "none") != 0 &&
            s.compare (f, n, "unknown") != 0)
          vendor.assign (s, f, n);
      }
    }

netbsd_empty_vendor:

    // (l, npos) is SYSTEM
    //
    system.assign (s, ++l, string::npos);

    if (system.empty ())
      bad ("missing os/kernel/abi");

    if (system.compare(0, 6, "netbsd") == 0)
       vendor.assign("unknown");
    else
       bad("empty vendor");

    if (system.front () == '-' || system.back () == '-')
      bad ("invalid os/kernel/abi");

    // Extract VERSION for some recognized systems.
    //
    string::size_type v (0);
    if (system.compare (0, (v = 6),  "darwin") == 0      ||
        system.compare (0, (v = 7),  "freebsd") == 0     ||
        system.compare (0, (v = 7),  "openbsd") == 0     ||
        system.compare (0, (v = 6),  "netbsd") == 0      ||
        system.compare (0, (v = 7),  "solaris") == 0     ||
        system.compare (0, (v = 3),  "aix") == 0         ||
        system.compare (0, (v = 4),  "hpux") == 0        ||
        system.compare (0, (v = 10), "win32-msvc") == 0  ||
        system.compare (0, (v = 12), "windows-msvc") == 0)
    {
      version.assign (system, v, string::npos);
      system.resize (system.size () - version.size ());
    }

    // Determine class for some recognized systems.
    //
    if (system.compare (0, 5, "linux") == 0)
      class_ = "linux";
    else if (vendor == "apple" && system == "darwin")
      class_ = "macos";
    else if (system == "freebsd" ||
             system == "openbsd" ||
             system == "netbsd")
      class_ = "bsd";
    else if (system.compare (0, 5, "win32") == 0   ||
             system.compare (0, 7, "windows") == 0 ||
             system == "mingw32")
      class_ = "windows";
    else
      class_ = "other";
  }

  std::string target_triplet::
  string () const
  {
    std::string r (cpu);

    if (!vendor.empty ())
    {
      if (!r.empty ()) r += '-';
      r += vendor;
    }

    if (!system.empty ())
    {
      if (!r.empty ()) r += '-';
      r += system;
    }

    if (!version.empty ())
    {
      r += version;
    }

    return r;
  }
}
