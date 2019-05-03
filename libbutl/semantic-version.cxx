// file      : libbutl/semantic-version.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules_ts
#include <libbutl/semantic-version.mxx>
#endif

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <string>
#include <cstddef>
#include <cstdint>
#include <ostream>

#include <cstring>   // strchr()
#include <cstdlib>   // strtoull()
#include <utility>   // move()
#include <stdexcept> // invalid_argument
#endif

// Other includes.

#ifdef __cpp_modules_ts
module butl.semantic_version;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules_ts
import std.core;
import std.io;
#endif
import butl.optional;
#endif
#else
#endif

using namespace std;

namespace butl
{
  string semantic_version::
  string (bool ib) const
  {
    std::string r;
    r  = to_string (major);
    r += '.';
    r += to_string (minor);
    r += '.';
    r += to_string (patch);
    if (!ib) r += build;
    return r;
  }

  uint64_t semantic_version::
  numeric () const
  {
    if (const char* w = (major > 999 ? "major version greater than 999" :
                         minor > 999 ? "minor version greater than 999" :
                         patch > 999 ? "patch version greater than 999" :
                         nullptr))
      throw invalid_argument (w);

    // AAABBBCCC0000
    //   10000000000
    //      10000000
    //         10000
    //
    return (major * 10000000000) + (minor * 10000000) + (patch * 10000);
  }

  semantic_version::
  semantic_version (uint64_t n, std::string b)
      : build (move (b))
  {
    //      AAABBBCCC0000
    if (n > 9999999990000ULL || (n % 1000) != 0)
      throw invalid_argument ("invalid numeric representation");

    //        AAABBBCCC0000
    major = n / 10000000000 % 1000;
    minor = n /    10000000 % 1000;
    patch = n /       10000 % 1000;
  }

  semantic_version::
  semantic_version (const std::string& s, size_t p, const char* bs)
  {
    semantic_version_result r (parse_semantic_version_impl (s, p, bs));

    if (r.version)
      *this = move (*r.version);
    else
      throw invalid_argument (r.failure_reason);
  }

  // From standard-version.cxx
  //
  bool
  parse_uint64 (const string& s, size_t& p,
                uint64_t& r,
                uint64_t min = 0, uint64_t max = uint64_t (~0));

  semantic_version_result
  parse_semantic_version_impl (const string& s, size_t p, const char* bs)
  {
    auto bail = [] (string m)
    {
      return semantic_version_result {nullopt, move (m)};
    };

    semantic_version r;

    if (!parse_uint64 (s, p, r.major))
      return bail ("invalid major version");

    if (s[p] != '.')
      return bail ("'.' expected after major version");

    if (!parse_uint64 (s, ++p, r.minor))
      return bail ("invalid minor version");

    if (s[p] == '.')
    {
      // Treat it as build if failed to parse as patch (e.g., 1.2.alpha).
      //
      if (!parse_uint64 (s, ++p, r.patch))
      {
        //if (require_patch)
        //  return bail ("invalid patch version");

        --p;
        // Fall through.
      }
    }
    //else if (require_patch)
    //  return bail ("'.' expected after minor version");

    if (char c = s[p])
    {
      if (bs == nullptr || (*bs != '\0' && strchr (bs, c) == nullptr))
        return bail ("junk after version");

      r.build.assign (s, p, string::npos);
    }

    return semantic_version_result {move (r), string ()};
  }
}
