// file      : libbutl/semantic-version.ixx -*- C++ -*-
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
  semantic_version (const std::string& s, flags fs, const char* bs)
      : semantic_version (s, 0, fs, bs)
  {
  }

  struct semantic_version_result
  {
    optional<semantic_version> version;
    std::string failure_reason;
  };

  LIBBUTL_SYMEXPORT semantic_version_result
  parse_semantic_version_impl (const std::string&,
                               std::size_t,
                               semantic_version::flags,
                               const char*);

  inline optional<semantic_version>
  parse_semantic_version (const std::string& s,
                          semantic_version::flags fs,
                          const char* bs)
  {
    return parse_semantic_version_impl (s, 0, fs, bs).version;
  }

  inline optional<semantic_version>
  parse_semantic_version (const std::string& s,
                          std::size_t p,
                          semantic_version::flags fs,
                          const char* bs)
  {
    return parse_semantic_version_impl (s, p, fs, bs).version;
  }

  inline semantic_version::flags
  operator&= (semantic_version::flags& x, semantic_version::flags y)
  {
    return x = static_cast<semantic_version::flags> (
      static_cast<std::uint16_t> (x) &
      static_cast<std::uint16_t> (y));
  }

  inline semantic_version::flags
  operator|= (semantic_version::flags& x, semantic_version::flags y)
  {
    return x = static_cast<semantic_version::flags> (
      static_cast<std::uint16_t> (x) |
      static_cast<std::uint16_t> (y));
  }

  inline semantic_version::flags
  operator& (semantic_version::flags x, semantic_version::flags y)
  {
    return x &= y;
  }

  inline semantic_version::flags
  operator| (semantic_version::flags x, semantic_version::flags y)
  {
    return x |= y;
  }
}
