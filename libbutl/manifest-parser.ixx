// file      : libbutl/manifest-parser.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace butl
{

  inline auto manifest_parser::
  get (const char* what) -> xchar
  {
    xchar c (base::get (ebuf_));

    if (invalid (c))
      throw manifest_parsing (name_,
                              c.line, c.column,
                              std::string ("invalid ") + what + ": " + ebuf_);
    return c;
  }

  inline void manifest_parser::
  get (const xchar& peeked)
  {
    base::get (peeked);
  }

  inline auto manifest_parser::
  peek (const char* what) -> xchar
  {
    xchar c (base::peek (ebuf_));

    if (invalid (c))
      throw manifest_parsing (name_,
                              c.line, c.column,
                              std::string ("invalid ") + what + ": " + ebuf_);
    return c;
  }

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
