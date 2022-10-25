// file      : libbutl/semantic-version.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/semantic-version.hxx>

#include <cassert>
#include <cstring>   // strchr()
#include <utility>   // move()
#include <stdexcept> // invalid_argument

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
    if (const char* w = (major > 99999 ? "major version greater than 99999" :
                         minor > 99999 ? "minor version greater than 99999" :
                         patch > 99999 ? "patch version greater than 99999" :
                         nullptr))
      throw invalid_argument (w);

    //          AAAAABBBBBCCCCC0000         BBBBBCCCCC0000         CCCCC0000
    return (major * 100000000000000) + (minor * 1000000000) + (patch * 10000);
  }

  semantic_version::
  semantic_version (uint64_t n, std::string b)
      : build (move (b))
  {
    //      AAAAABBBBBCCCCC0000
    if (n > 9999999999999990000ULL || (n % 10000) != 0)
      throw invalid_argument ("invalid numeric representation");

    //      AAAAABBBBBCCCCC0000
    major = n / 100000000000000 % 100000;
    minor = n /      1000000000 % 100000;
    patch = n /           10000 % 100000;
  }

  semantic_version::
  semantic_version (const std::string& s, size_t p, flags fs, const char* bs)
  {
    semantic_version_result r (parse_semantic_version_impl (s, p, fs, bs));

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
  parse_semantic_version_impl (const string& s, size_t p,
                               semantic_version::flags fs,
                               const char* bs)
  {
    bool allow_build ((fs & semantic_version::allow_build) != 0);

    // If build separators are specified, then the allow_build flag must be
    // specified explicitly.
    //
    assert (bs == nullptr || allow_build);

    if (allow_build && bs == nullptr)
      bs = "-+";

    bool require_minor ((fs & semantic_version::allow_omit_minor) == 0);

    if (!require_minor)
      fs |= semantic_version::allow_omit_patch;

    bool require_patch ((fs & semantic_version::allow_omit_patch) == 0);

    auto bail = [] (string m)
    {
      return semantic_version_result {nullopt, move (m)};
    };

    semantic_version r;

    if (!parse_uint64 (s, p, r.major))
      return bail ("invalid major version");

    if (s[p] == '.') // Is there a minor version?
    {
      // Try to parse the minor version and treat it as build on failure
      // (e.g., 1.alpha).
      //
      if (parse_uint64 (s, ++p, r.minor))
      {
        if (s[p] == '.') // Is there a patch version?
        {
          // Try to parse the patch version and treat it as build on failure
          // (e.g., 1.2.alpha).
          //
          if (parse_uint64 (s, ++p, r.patch))
            ;
          else
          {
            if (require_patch)
              return bail ("invalid patch version");

            --p;
            // Fall through.
          }
        }
        else if (require_patch)
          return bail ("'.' expected after minor version");
      }
      else
      {
        if (require_minor)
          return bail ("invalid minor version");

        --p;
        // Fall through.
      }
    }
    else if (require_minor)
      return bail ("'.' expected after major version");

    if (char c = s[p])
    {
      if (!allow_build || (*bs != '\0' && strchr (bs, c) == nullptr))
        return bail ("junk after version");

      r.build.assign (s, p, string::npos);
    }

    return semantic_version_result {move (r), string ()};
  }
}
