// file      : libbutl/standard-version.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_STANDARD_VERSION_HXX
#define LIBBUTL_STANDARD_VERSION_HXX

#include <string>
#include <cstdint> // uint*_t
#include <cstddef> // size_t
#include <ostream>

#include <libbutl/export.hxx>

#include <libbutl/optional.hxx>

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
  // The build2 "standard version" (specific, earliest and stub):
  //
  // [<epoch>~]<maj>.<min>.<patch>[-(a|b).<num>[.<snapsn>[.<snapid>]]][+<rev>]
  // [<epoch>~]<maj>.<min>.<patch>-
  // 0[+<revision>]
  //
  struct LIBBUTL_EXPORT standard_version
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

    std::uint16_t epoch        = 0;  // 0 if not specified.
    std::uint64_t version      = 0;  // AAABBBCCCDDDE
    std::uint64_t snapshot_sn  = 0;  // 0 if not specifed, latest_sn if 'z'.
    std::string   snapshot_id;       // Empty if not specified.
    std::uint16_t revision     = 0;  // 0 if not specified.

    std::uint16_t major () const noexcept;
    std::uint16_t minor () const noexcept;
    std::uint16_t patch () const noexcept;

    // Note: 0 is ambiguous (-a.0.z).
    //
    std::uint16_t pre_release () const noexcept;

    // Note: return empty if the corresponding component is unspecified.
    //
    std::string string () const;             // Package version.
    std::string string_project () const;     // Project version (no epoch/rev).
    std::string string_project_id () const;  // Project version id (no snapsn).
    std::string string_version () const;     // Version only (no snapshot).
    std::string string_pre_release () const; // Pre-release part only (a.1).
    std::string string_snapshot () const;    // Snapshot part only (1234.1f23).

    bool empty () const noexcept {return version == 0;}

    bool alpha () const noexcept;
    bool beta () const noexcept;
    bool snapshot () const noexcept {return snapshot_sn != 0;}

    // Represented by DDDE in version being 0001 and snapshot_sn being 0.
    //
    // Note that the earliest version is a final alpha pre-release.
    //
    bool
    earliest () const noexcept;

    bool
    stub () const noexcept {return version == std::uint64_t (~0);}

    // Comparison of empty or stub versions doesn't make sense.
    //
    int
    compare (const standard_version& v) const noexcept
    {
      if (epoch != v.epoch)
        return epoch < v.epoch ? -1 : 1;

      if (version != v.version)
        return version < v.version ? -1 : 1;

      if (snapshot_sn != v.snapshot_sn)
        return snapshot_sn < v.snapshot_sn ? -1 : 1;

      if (revision != v.revision)
        return revision < v.revision ? -1 : 1;

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

    // Create empty version.
    //
    standard_version () = default;

  private:
    void
    parse_snapshot (const std::string&, std::size_t&);
  };

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
  // ('(' | '[') <version> <version> (')' | ']')
  //
  struct LIBBUTL_EXPORT standard_version_constraint
  {
    butl::optional<standard_version> min_version;
    butl::optional<standard_version> max_version;
    bool min_open;
    bool max_open;

    // Parse the version constraint. Throw std::invalid_argument on error.
    //
    explicit
    standard_version_constraint (const std::string&);

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

#endif // LIBBUTL_STANDARD_VERSION_HXX