// file      : libbutl/target-triplet.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <ostream>

#include <libbutl/export.hxx>

namespace butl
{
  // This is the ubiquitous 'target triplet' that loosely has the CPU-VENDOR-OS
  // form which, these days, quite often takes the CPU-VENDOR-OS-ABI form. Plus
  // some fields can sometimes be omitted. This looseness makes it hard to base
  // any kind of decisions on the triplet without canonicalizing it and then
  // splitting it into components. The way we are going to split it is like
  // this:
  //
  // CPU
  //
  // This one is reasonably straightforward. Note that we always expect at
  // least two components with the first being the CPU. In other words, we
  // don't try to guess what just 'mingw32' might mean like config.sub does.
  // Note that we canonicalize arm64 to aarch64 similar to config.sub.
  //
  // VENDOR
  //
  // This can be a machine vendor as in i686-apple-darwin8, a toolchain vendor
  // as in i686-lfs-linux-gnu, or something else as in arm-softfloat-linux-gnu.
  // Just as we think vendor is pretty irrelevant and can be ignored, comes
  // MinGW-W64 and calls itself *-w64-mingw32. While it is tempting to
  // attribute w64 to OS-ABI, the MinGW-W64 folks insist it is a (presumably
  // toolchain) vendor.
  //
  // Another example where the vendor seems to be reused for something else
  // entirely is the Intel's MIC architecture: x86_64-k1om-linux.
  //
  // To make things more regular we also convert the information-free vendor
  // names 'pc', 'unknown' and 'none' to the empty name.
  //
  // OS/KERNEL-OS/OS-ABI
  //
  // This is where things get really messy and instead of trying to guess, we
  // call the entire thing SYSTEM. Except, in certain cases, we factor out the
  // trailing version, again, to make SYSTEM easier to compare to. For example,
  // *-darwin14.5.0 becomes 'darwin' and '14.5.0'.
  //
  // Note also that sometimes the first component in SYSTEM can be 'none' (to
  // indicate the absence of an operating system) which is ambigous with the
  // vendor (for example, arm-none-eabi). We currently don't try to deal with
  // that (that is, you will need to specify arm-unknown-none-eabi).
  //
  // Values for two-component systems (e.g., linux-gnu) that don't specify
  // VENDOR explicitly are inherently ambiguous: is 'linux' VENDOR or part of
  // SYSTEM? The only way to handle this is to recognize their specific names
  // as special cases and this is what we do for some of the more common
  // ones. The alternative would be to first run such names through config.sub
  // which adds explicit VENDOR and this could be a reasonable fallback
  // strategy for (presumably less common) cases were we don't split things
  // correctly.
  //
  // Note also that the version splitting is only done for certain commonly-
  // used targets.
  //
  // Some examples of canonicalization and splitting:
  //
  // x86_64-apple-darwin14.5.0         x86_64  apple      darwin         14.5.0
  // x86_64-unknown-freebsd10.2        x86_64             freebsd        10.2
  // x86_64-unknown-netbsd9.0          x86_64             netbsd         9.0
  // i686-elf                          i686               elf
  // arm-eabi                          arm                eabi
  // arm-none-eabi                     arm                eabi
  // arm-none-linux-gnueabi            arm                linux-gnueabi
  // arm-softfloat-linux-gnu           arm     softfloat  linux-gnu
  // i686-pc-mingw32                   i686               mingw32
  // i686-w64-mingw32                  i686    w64        mingw32
  // i686-w64-windows-gnu              i686    w64        mingw32
  // i686-lfs-linux-gnu                i686    lfs        linux-gnu
  // x86_64-unknown-linux-gnu          x86_64             linux-gnu
  // x86_64-redhat-linux               x86_64  redhat     linux-gnu
  // x86_64-linux-gnux32               x86_64             linux-gnux32
  // x86_64-microsoft-win32-msvc14.0   x86_64  microsoft  win32-msvc     14.0
  // x86_64-pc-windows-msvc            x86_64             windows-msvc
  // x86_64-pc-windows-msvc19.11.25547 x86_64             windows-msvc   19.11.25547
  // wasm32-unknown-emscripten         wasm32             emscripten
  // arm64-apple-darwin20.1.0          aarch64 apple      darwin         20.1.0
  // arm64-apple-ios14.4               aarch64 apple      ios            14.4
  // arm64-apple-ios14.4-simulator     aarch64 apple      ios-simulator  14.4
  // x86_64-apple-ios14.4-macabi       x86_64  apple      ios-macabi     14.4
  //
  // Similar to version splitting, for certain commonly-used targets we also
  // derive the "target class" which can be used as a shorthand, more
  // convenient way to identify a targets. If the target is not recognized,
  // then the special 'other' value is used. Currently the following classes
  // are recognized:
  //
  // linux     *-*-linux-*
  // macos     *-apple-darwin*
  // bsd       *-*-(freebsd|openbsd|netbsd)*
  // windows   *-*-win32-* | *-*-windows-* | *-*-mingw32
  // ios       *-apple-ios*
  //
  // NOTE: see also os_release if adding anything new here.
  //
  // References:
  //
  // 1. The libtool repository contains the PLATFORM file that lists many known
  //    triplets.
  //
  // 2. LLVM has the Triple class with similar goals.
  //
  struct LIBBUTL_SYMEXPORT target_triplet
  {
    std::string cpu;
    std::string vendor;
    std::string system;
    std::string version;
    std::string class_;

    // Assemble and returning the canonical (i.e., without unknown vendor)
    // target triplet string.
    //
    // Note: not necessarily round-tripp'able, see representation().
    //
    std::string
    string () const;

    // Return a round-tripp'able target triplet string that always contains
    // the vendor.
    //
    std::string
    representation () const;

    bool
    empty () const {return cpu.empty ();}

    int
    compare (const target_triplet& y) const
    {
      int r;
      return
        (r =    cpu.compare (y.cpu))    != 0 ? r :
        (r = vendor.compare (y.vendor)) != 0 ? r :
        (r = system.compare (y.system)) != 0 ? r :
        (   version.compare (y.version));
    }

    // Parse the triplet throw std::invalid_argument if the triplet is not
    // recognizable.
    //
    explicit
    target_triplet (const std::string&);

    target_triplet () = default;
  };

  inline bool
  operator== (const target_triplet& x, const target_triplet& y)
  {
    return x.compare (y) == 0;
  }

  inline bool
  operator!= (const target_triplet& x, const target_triplet& y)
  {
    return !(x == y);
  }

  inline std::ostream&
  operator<< (std::ostream& o, const target_triplet& x)
  {
    return o << x.string ();
  }
}
