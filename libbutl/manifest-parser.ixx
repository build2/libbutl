// file      : libbutl/manifest-parser.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline manifest_name_value manifest_parser::
  next ()
  {
    manifest_name_value r;
    do { parse_next (r); } while (filter_ && !filter_ (r));
    return r;
  }

  inline optional<std::vector<manifest_name_value>>
  try_parse_manifest (manifest_parser& p)
  {
    std::vector<manifest_name_value> r;
    return try_parse_manifest (p, r)
           ? optional<std::vector<manifest_name_value>> (move (r))
           : nullopt;
  }

  inline std::vector<manifest_name_value>
  parse_manifest (manifest_parser& p)
  {
    std::vector<manifest_name_value> r;
    parse_manifest (p, r);
    return r;
  }
}
