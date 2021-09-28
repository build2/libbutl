// file      : libbutl/tab-parser.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/tab-parser.hxx>

#include <istream>
#include <sstream>

#include <libbutl/string-parser.hxx>

using namespace std;

namespace butl
{
  using parsing = tab_parsing;

  // tab_parser
  //
  tab_fields tab_parser::
  next ()
  {
    tab_fields r;

    // Read lines until a non-empty one or EOF is encountered. In the first
    // case parse the line and bail out.
    //
    // Note that we check for character presence in the stream prior to the
    // getline() call, to prevent it from setting the failbit.
    //
    while (!is_.eof () && is_.peek () != istream::traits_type::eof ())
    {
      string s;
      getline (is_, s);

      ++line_;

      // Skip empty line.
      //
      auto i (s.begin ());
      auto e (s.end ());
      for (; i != e && (*i == ' ' || *i == '\t'); ++i) ; // Skip spaces.

      if (i == e || *i == '#')
        continue;

      r.line = line_;
      r.end_column = s.size () + 1; // Newline position.

      vector<std::pair<string, size_t>> sp;

      try
      {
        sp = string_parser::parse_quoted_position (s, false);
      }
      catch (const string_parser::invalid_string& e)
      {
        throw parsing (name_, line_, e.position + 1, e.what ());
      }

      for (auto& s: sp)
        r.emplace_back (tab_field ({move (s.first), s.second + 1}));

      break;
    }

    return r;
  }

  // tab_parsing
  //
  static inline string
  format (const string& n, uint64_t l, uint64_t c, const string& d)
  {
    ostringstream os;
    if (!n.empty ())
      os << n << ':';
    os << l << ':' << c << ": error: " << d;
    return os.str ();
  }

  tab_parsing::
  tab_parsing (const string& n, uint64_t l, uint64_t c, const string& d)
      : runtime_error (format (n, l, c, d)),
        name (n), line (l), column (c), description (d)
  {
  }
}
