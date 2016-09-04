// file      : butl/triplet.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/triplet>

#include <stdexcept> // invalid_argument

using namespace std;

namespace butl
{
  triplet::
  triplet (const string& s, string* c)
  {
    auto bad = [](const char* m) {throw invalid_argument (m);};

    // Find the first and the last components. The first is CPU and the last is
    // (part of) SYSTEM, that we know for sure.
    //
    string::size_type f (s.find ('-')), l (s.rfind ('-'));

    if (f == 0 || f == string::npos)
      bad ("missing cpu");

    cpu.assign (s, 0, f);

    if (c != nullptr)
      *c = cpu;

    // If we have something in between, then the first component after CPU is
    // VENDOR. Unless it is a first component of two-component system, as in
    // i686-linux-gnu.
    //
    if (f != l)
    {
      // [f, p) is VENDOR.
      //
      string::size_type p (s.find ('-', ++f)), n (p - f);

      if (n == 0)
        bad ("empty vendor");

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
        if (s.compare (f, n, "linux") == 0 ||
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
        {
          vendor.assign (s, f, n);

          if (c != nullptr)
          {
            *c += '-';
            *c += vendor;
          }
        }
      }
    }

    // (l, npos) is SYSTEM
    //
    system.assign (s, ++l, string::npos);

    if (system.empty ())
      bad ("missing os/kernel/abi");

    if (system.front () == '-' || system.back () == '-')
      bad ("invalid os/kernel/abi");

    if (c != nullptr)
    {
      *c += '-';
      *c += system;
    }

    // Extract VERSION for some recognized systems.
    //
    string::size_type v (0);
    if (system.compare (0, (v = 6),  "darwin") == 0  ||
        system.compare (0, (v = 7),  "freebsd") == 0 ||
        system.compare (0, (v = 7),  "openbsd") == 0 ||
        system.compare (0, (v = 6),  "netbsd") == 0  ||
        system.compare (0, (v = 7),  "solaris") == 0 ||
        system.compare (0, (v = 3),  "aix") == 0     ||
        system.compare (0, (v = 4),  "hpux") == 0    ||
        system.compare (0, (v = 10), "win32-msvc") == 0)
    {
      version.assign (system, v, string::npos);
      system.resize (system.size () - version.size ());
    }

    // Determine class for some recognized systems.
    //
    if (system.compare (0, 5, "linux") == 0)
      class_ = "linux";
    else if (vendor == "apple" && system == "darwin")
      class_ = "macosx";
    else if (system == "freebsd" || system == "openbsd" || system == "netbsd")
      class_ = "bsd";
    else if (system.compare (0, 5, "win32") == 0 || system == "mingw32")
      class_ = "windows";
    else
      class_ = "other";
  }
}
