// file      : libbutl/standard-version.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <cstdint> // uint*_t
#include <cstddef> // size_t
#include <ostream>

#include <libbutl/optional.hxx>

#include <libbutl/export.hxx>

// FreeBSD defines these macros in its <sys/types.h>.
//
#ifdef major
#  undef major
#endif

#ifdef minor
#  undef minor
#endif

namespace butl
{
  // The build2 "standard version" (normal, earliest, and stub):
  //
  // [+<epoch>-]<maj>.<min>.<patch>[-(a|b).<num>[.<snapsn>[.<snapid>]]][+<rev>]
  // [+<epoch>-]<maj>.<min>.<patch>-
  // 0[+<rev>]
  //
  // The normal version can be release, final pre-release, or a pre-release
  // snapshot (release is naturally always final). Pre-release can be alpha or
  // beta.
  //
  // The numeric version format is AAAAABBBBBCCCCCDDDE where:
  //
  // AAAAA - major version number
  // BBBBB - minor version number
  // CCCCC - patch version number
  // DDD   - alpha / beta (DDD + 500) version number
  // E     - final (0) / snapshot (1)
  //
  // When DDDE is not 0, 1 is subtracted from AAAAABBBBBCCCCC. For example:
  //
  // Version      AAAAABBBBBCCCCCDDDE
  //
  // 0.1.0        0000000001000000000
  // 0.1.2        0000000001000020000
  // 1.2.3        0000100002000030000
  // 2.2.0-a.1    0000200001999990010
  // 3.0.0-b.2    0000299999999995020
  // 2.2.0-a.1.z  0000200001999990011
  //
  // Stub is represented as ~0 (but is not considered a pre-release).
  //
  struct LIBBUTL_SYMEXPORT standard_version
  {
    // Invariants:
    //
    // 1. allow_earliest
    //    ? (E == 1) || (snapshot_sn == 0)
    //    : (E == 0) == (snapshot_sn == 0)
    //
    // 2. version != 0 || allow_stub && epoch == 0 && snapshot_sn == 0
    //
    // 3. snapshot_sn != latest_sn && snapshot_sn != 0 || snapshot_id.empty ()
    //
    static const std::uint64_t latest_sn = std::uint64_t (~0);

    std::uint16_t epoch        = 1;  // 0 if a stub, 1 if not specified.
    std::uint64_t version      = 0;  // AAAAABBBBBCCCCCDDDE or ~0 for stub.
    std::uint64_t snapshot_sn  = 0;  // 0 if not specifed, latest_sn if 'z'.
    std::string   snapshot_id;       // Empty if not specified.
    std::uint16_t revision     = 0;  // 0 if not specified.

    std::uint32_t major () const noexcept;
    std::uint32_t minor () const noexcept;
    std::uint32_t patch () const noexcept;

    // Return the alpha/beta version number if pre-release and nullopt
    // otherwise.
    //
    // Can be used as a predicate and also to get the value.
    //
    optional<std::uint16_t> alpha () const noexcept;
    optional<std::uint16_t> beta () const noexcept;

    // Return the DDD version part if a pre-release and nullopt otherwise.
    //
    // Can be used as a predicate and also to get the value. Note that 0 is
    // ambiguous (-[ab].0.z, or earliest version; see below).
    //
    optional<std::uint16_t> pre_release () const noexcept;

    // String representations.
    //
    // Note: return empty if the corresponding component is unspecified.
    //
    std::string string () const; // Package version.

    // Project version (no epoch).
    //
    std::string string_project (bool revision = false) const;

    std::string string_project_id () const;  // Project version id (no snapsn).
    std::string string_version () const;     // Version only (no snapshot).
    std::string string_pre_release () const; // Pre-release part only (a.1).
    std::string string_snapshot () const;    // Snapshot part only (1234.1f23).

    // Predicates. See also alpha(), beta(), and pre_release() above.
    //
    // The earliest version is represented as the (otherwise illegal) DDDE
    // value 0001 and snapshot_sn 0. Note that the earliest version is a final
    // alpha pre-release.
    //
    bool empty () const noexcept {return version == 0;}
    bool stub () const noexcept {return version == std::uint64_t (~0);}
    bool earliest () const noexcept;
    bool release () const noexcept;
    bool snapshot () const noexcept {return snapshot_sn != 0;}
    bool latest_snapshot () const noexcept;
    bool final () const noexcept;

    // Comparison of empty or stub versions doesn't make sense.
    //
    int
    compare (const standard_version& v,
             bool ignore_revision = false) const noexcept
    {
      if (epoch != v.epoch)
        return epoch < v.epoch ? -1 : 1;

      if (version != v.version)
        return version < v.version ? -1 : 1;

      if (snapshot_sn != v.snapshot_sn)
        return snapshot_sn < v.snapshot_sn ? -1 : 1;

      if (!ignore_revision)
      {
        if (revision != v.revision)
          return revision < v.revision ? -1 : 1;
      }

      return 0;
    }

    // Parse the version. Throw std::invalid_argument if the format is not
    // recognizable or components are invalid.
    //
    enum flags
    {
      none           = 0,
      allow_earliest = 0x01, // Allow <major>.<minor>.<patch>- form.
      allow_stub     = 0x02  // Allow 0[+<revision>] form.
    };

    explicit
    standard_version (const std::string&, flags = none);

    explicit
    standard_version (std::uint64_t version, flags = none);

    standard_version (std::uint64_t version,
                      const std::string& snapshot,
                      flags = none);

    // Note that the default epoch is 1 for real versions and 0 for stubs.
    //
    standard_version (std::uint16_t epoch,
                      std::uint64_t version,
                      const std::string& snapshot,
                      std::uint16_t revision,
                      flags = none);

    standard_version (std::uint16_t epoch,
                      std::uint64_t version,
                      std::uint64_t snapshot_sn,
                      std::string snapshot_id,
                      std::uint16_t revision,
                      flags = none);

    // Version as separate major, minor, patch, and pre-release components.
    // Note that the pre-release here is in the DDD form, that is, incremented
    // by 500 for betas.
    //
    standard_version (std::uint16_t epoch,
                      std::uint32_t major,
                      std::uint32_t minor,
                      std::uint32_t patch,
                      std::uint16_t pre_release = 0,
                      std::uint16_t revision = 0);

    standard_version (std::uint16_t epoch,
                      std::uint32_t major,
                      std::uint32_t minor,
                      std::uint32_t patch,
                      std::uint16_t pre_release,
                      std::uint64_t snapshot_sn,
                      std::string snapshot_id,
                      std::uint16_t revision = 0);

    // Create empty version.
    //
    standard_version () = default;
  };

  // Try to parse a string as a standard version returning nullopt if invalid.
  //
  LIBBUTL_SYMEXPORT optional<standard_version>
  parse_standard_version (const std::string&,
                          standard_version::flags = standard_version::none);

  inline bool
  operator< (const standard_version& x, const standard_version& y) noexcept
  {
    return x.compare (y) < 0;
  }

  inline bool
  operator> (const standard_version& x, const standard_version& y) noexcept
  {
    return x.compare (y) > 0;
  }

  inline bool
  operator== (const standard_version& x, const standard_version& y) noexcept
  {
    return x.compare (y) == 0;
  }

  inline bool
  operator<= (const standard_version& x, const standard_version& y) noexcept
  {
    return x.compare (y) <= 0;
  }

  inline bool
  operator>= (const standard_version& x, const standard_version& y) noexcept
  {
    return x.compare (y) >= 0;
  }

  inline bool
  operator!= (const standard_version& x, const standard_version& y) noexcept
  {
    return !(x == y);
  }

  inline std::ostream&
  operator<< (std::ostream& o, const standard_version& x)
  {
    return o << x.string ();
  }

  inline standard_version::flags
  operator& (standard_version::flags, standard_version::flags);

  inline standard_version::flags
  operator| (standard_version::flags, standard_version::flags);

  inline standard_version::flags
  operator&= (standard_version::flags&, standard_version::flags);

  inline standard_version::flags
  operator|= (standard_version::flags&, standard_version::flags);

  // The build2 "standard version" constraint:
  //
  // ('==' | '>' | '<' | '>=' | '<=') <version>
  // ('^' | '~') <version>
  // ('(' | '[') <version> <version> (')' | ']')
  //
  // The version may be `$` which refers to the dependent package version.
  //
  struct LIBBUTL_SYMEXPORT standard_version_constraint
  {
    butl::optional<standard_version> min_version;
    butl::optional<standard_version> max_version;
    bool min_open;
    bool max_open;

    // Parse the version constraint. Throw std::invalid_argument on error.
    //
    explicit
    standard_version_constraint (const std::string&);

    // As above but also completes the special `$` version using the specified
    // dependent package version.
    //
    standard_version_constraint (const std::string&,
                                 const standard_version& dependent_version);

    // Throw std::invalid_argument if the specified version range is invalid.
    //
    standard_version_constraint (
      butl::optional<standard_version> min_version, bool min_open,
      butl::optional<standard_version> max_version, bool max_open);

    explicit
    standard_version_constraint (const standard_version& v)
        : standard_version_constraint (v, false, v, false) {}

    standard_version_constraint () = default;

    std::string
    string () const;

    bool
    empty () const noexcept {return !min_version && !max_version;}

    bool
    satisfies (const standard_version&) const noexcept;
  };

  inline bool
  operator== (const standard_version_constraint& x,
              const standard_version_constraint& y)
  {
    return x.min_version == y.min_version && x.max_version == y.max_version &&
      x.min_open == y.min_open && x.max_open == y.max_open;
  }

  inline bool
  operator!= (const standard_version_constraint& x,
              const standard_version_constraint& y)
  {
    return !(x == y);
  }

  inline std::ostream&
  operator<< (std::ostream& o, const standard_version_constraint& x)
  {
    return o << x.string ();
  }
}

#include <libbutl/standard-version.ixx>
