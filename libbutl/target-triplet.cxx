// file      : libbutl/target-triplet.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/target-triplet.hxx>

#include <stdexcept> // invalid_argument

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

    // Canonicalize CPU.
    //
    if (s.compare (0, f, "arm64") == 0)
      cpu = "aarch64";
    else
      cpu.assign (s, 0, f);

    // If we have something in between, then the first component after CPU is
    // VENDOR. Unless it is a first component of two-component system, as in
    // i686-linux-gnu.
    //
    // There are also cases like x86_64--netbsd.
    //
    if (l - f > 1)
    {
      // [f, p) is VENDOR.
      //
      string::size_type p (s.find ('-', ++f)), n (p - f);

      // Do we have all four components? If so, then we don't need to do any
      // special recognition of two-component systems.
      //
      if (l != p)
      {
        l = s.rfind ('-', --l);

        if (l != p)
          bad ("too many components");
      }
      else
      {
        // See if this is one of the well-known non-vendors.
        //
        if (s.compare (f, n, "linux") == 0    ||
            s.compare (f, n, "windows") == 0  ||
            s.compare (f, n, "kfreebsd") == 0 ||
            s.compare (f, n, "nto") == 0)
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

    // (l, npos) is SYSTEM
    //
    system.assign (s, ++l, string::npos);

    if (system.empty ())
      bad ("missing os/kernel/abi");

    if (system.front () == '-' || system.back () == '-')
      bad ("invalid os/kernel/abi");

    // Canonicalize SYSTEM.
    //
    if (system == "linux")
      system = "linux-gnu"; // Per config.sub.
    else if (system == "windows-gnu" && vendor == "w64") // Clang's innovation.
      system = "mingw32";

    // Extract VERSION for some recognized systems.
    //
    string::size_type v (0);
    if (system.compare (0, (v = 6),  "darwin") == 0       ||
        system.compare (0, (v = 7),  "freebsd") == 0      ||
        system.compare (0, (v = 7),  "openbsd") == 0      ||
        system.compare (0, (v = 6),  "netbsd") == 0       ||
        system.compare (0, (v = 7),  "solaris") == 0      ||
        system.compare (0, (v = 3),  "aix") == 0          ||
        system.compare (0, (v = 4),  "hpux") == 0         ||
        system.compare (0, (v = 10), "win32-msvc") == 0   ||
        system.compare (0, (v = 12), "windows-msvc") == 0 ||
        system.compare (0, (v = 7),  "nto-qnx") == 0)
    {
      version.assign (system, v, string::npos);
      system.resize (system.size () - version.size ());
    }
    else if (vendor == "apple" && system.compare (0, 3, "ios") == 0)
    {
      // Handle iosNN[-...].
      //
      string::size_type p (system.find ('-'));
      version.assign (system, 3, p == string::npos ? p : p - 3);
      system.erase (3, version.size ());
    }

    // Determine class for some recognized systems.
    //
    if (system.compare (0, 5, "linux") == 0)
      class_ = "linux";
    else if (vendor == "apple" && system == "darwin")
      class_ = "macos";
    else if (vendor == "apple" && system.compare (0, 3, "ios") == 0)
      class_ = "ios";
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
      if (vendor == "apple" && system.compare (0, 3, "ios") == 0)
        r.insert (r.size () - system.size () + 3, version);
      else
        r += version;
    }

    return r;
  }

  std::string target_triplet::
  representation () const
  {
    std::string r (cpu);

    {
      if (!r.empty ()) r += '-';
      r += vendor.empty () ? "unknown" : vendor.c_str ();
    }

    if (!system.empty ())
    {
      if (!r.empty ()) r += '-';
      r += system;
    }

    if (!version.empty ())
    {
      if (vendor == "apple" && system.compare (0, 3, "ios") == 0)
        r.insert (r.size () - system.size () + 3, version);
      else
        r += version;
    }

    return r;
  }
}
