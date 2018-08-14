// file      : libbutl/semantic-version.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline semantic_version::
  semantic_version (std::uint64_t mj,
                    std::uint64_t mi,
                    std::uint64_t p,
                    std::string   b)
      : major (mj),
        minor (mi),
        patch (p),
        build (std::move (b))
  {
  }

  inline semantic_version::
  semantic_version (const std::string& s, bool ab)
      : semantic_version (s, ab ? "-+" : nullptr)
  {
  }

  inline semantic_version::
  semantic_version (const std::string& s, const char* bs)
      : semantic_version (s, 0, bs)
  {
  }

  inline semantic_version::
  semantic_version (const std::string& s, std::size_t p, bool ab)
      : semantic_version (s, p, ab ? "-+" : nullptr)
  {
  }

  inline int semantic_version::
  compare (const semantic_version& v, bool ib) const
  {
    return (major != v.major ? (major < v.major ? -1 : 1) :
            minor != v.minor ? (minor < v.minor ? -1 : 1) :
            patch != v.patch ? (patch < v.patch ? -1 : 1) :
            ib ? 0 : build.compare (v.build));
  }

  struct semantic_version_result
  {
    optional<semantic_version> version;
    std::string failure_reason;
  };

  LIBBUTL_SYMEXPORT semantic_version_result
  parse_semantic_version_impl (const std::string&, std::size_t, const char*);

  inline optional<semantic_version>
  parse_semantic_version (const std::string& s, bool ab)
  {
    return parse_semantic_version (s, ab ? "-+" : nullptr);
  }

  inline optional<semantic_version>
  parse_semantic_version (const std::string& s, const char* bs)
  {
    return parse_semantic_version_impl (s, 0, bs).version;
  }

  inline optional<semantic_version>
  parse_semantic_version (const std::string& s, std::size_t p, bool ab)
  {
    return parse_semantic_version (s, p, ab ? "-+" : nullptr);
  }

  inline optional<semantic_version>
  parse_semantic_version (const std::string& s, std::size_t p, const char* bs)
  {
    return parse_semantic_version_impl (s, p, bs).version;
  }
}
