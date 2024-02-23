// file      : libbutl/semantic-version.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <cstddef> // size_t
#include <cstdint> // uint*_t
#include <utility> // move()
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
  // Semantic or semantic-like version.
  //
  // <major>[.<minor>[.<patch>]][<build>]
  //
  // If the minor and patch components are absent, then they default to 0.
  //
  // By default, a version containing the <build> component is considered
  // valid only if separated from <patch> with '-' (semver pre-release) or '+'
  // (semver build metadata). However, as discussed below, the list of valid
  // separators can be customized to recognize other semver-like formats.
  //
  // Note also that the format of semver pre-release and build metadata are
  // not validated.
  //
  struct LIBBUTL_SYMEXPORT semantic_version
  {
    std::uint64_t major = 0;
    std::uint64_t minor = 0;
    std::uint64_t patch = 0;
    std::string   build;

    // Construct the semantic version from various representations. Throw
    // std::invalid_argument if the format is not recognizable or components
    // are invalid.
    //
    semantic_version () = default;

    semantic_version (std::uint64_t major,
                      std::uint64_t minor,
                      std::uint64_t patch,
                      std::string   build = "");

    // If the allow_build flag is specified, then build_separators argument
    // can be a string of characters to allow as separators, empty (any build
    // component allowed), or NULL (defaults to "-+").
    //
    // Note: allow_omit_minor implies allow_omit_patch.
    //
    enum flags
    {
      none             = 0,    // Exact <major>.<minor>.<patch> form.
      allow_omit_minor = 0x01, // Allow <major> form.
      allow_omit_patch = 0x02, // Allow <major>.<minor> form.
      allow_build      = 0x04, // Allow <major>.<minor>.<patch>-<build> form.
    };

    explicit
    semantic_version (const std::string&,
                      flags = none,
                      const char* build_separators = nullptr);

    // As above but parse from the specified position until the end of the
    // string.
    //
    semantic_version (const std::string&,
                      std::size_t pos,
                      flags = none,
                      const char* = nullptr);

    // @@ We may also want to pass allow_* flags not to print 0 minor/patch or
    //    maybe invent ignore_* flags.
    //
    std::string
    string (bool ignore_build = false) const;

    // Numeric representation in the AAAAABBBBBCCCCC0000 form, where:
    //
    // AAAAA - major version number
    // BBBBB - minor version number
    // CCCCC - patch version number
    //
    // See standard version for details.
    //
    explicit
    semantic_version (std::uint64_t numeric, std::string build = "");

    // If any of the major/minor/patch components is greater than 99999, then
    // throw std::invalid_argument. The build component is ignored.
    //
    std::uint64_t
    numeric () const;

    // Unless instructed to ignore, the build components are compared
    // lexicographically.
    //
    int
    compare (const semantic_version& v, bool ignore_build = false) const
    {
      return (major != v.major ? (major < v.major ? -1 : 1) :
              minor != v.minor ? (minor < v.minor ? -1 : 1) :
              patch != v.patch ? (patch < v.patch ? -1 : 1) :
              ignore_build ? 0 : build.compare (v.build));
    }
  };

  // Try to parse a string as a semantic version returning nullopt if invalid.
  //
  optional<semantic_version>
  parse_semantic_version (const std::string&,
                          semantic_version::flags = semantic_version::none,
                          const char* build_separators = nullptr);

  optional<semantic_version>
  parse_semantic_version (const std::string&,
                          std::size_t pos,
                          semantic_version::flags = semantic_version::none,
                          const char* = nullptr);

  // NOTE: comparison operators take the build component into account.
  //
  inline bool
  operator< (const semantic_version& x, const semantic_version& y)
  {
    return x.compare (y) < 0;
  }

  inline bool
  operator> (const semantic_version& x, const semantic_version& y)
  {
    return x.compare (y) > 0;
  }

  inline bool
  operator== (const semantic_version& x, const semantic_version& y)
  {
    return x.compare (y) == 0;
  }

  inline bool
  operator<= (const semantic_version& x, const semantic_version& y)
  {
    return x.compare (y) <= 0;
  }

  inline bool
  operator>= (const semantic_version& x, const semantic_version& y)
  {
    return x.compare (y) >= 0;
  }

  inline bool
  operator!= (const semantic_version& x, const semantic_version& y)
  {
    return !(x == y);
  }

  inline std::ostream&
  operator<< (std::ostream& o, const semantic_version& x)
  {
    return o << x.string ();
  }

  semantic_version::flags
  operator& (semantic_version::flags, semantic_version::flags);

  semantic_version::flags
  operator| (semantic_version::flags, semantic_version::flags);

  semantic_version::flags
  operator&= (semantic_version::flags&, semantic_version::flags);

  semantic_version::flags
  operator|= (semantic_version::flags&, semantic_version::flags);
}

#include <libbutl/semantic-version.ixx>
