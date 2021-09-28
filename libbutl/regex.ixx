// file      : libbutl/regex.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <utility> // move(), make_pair()

namespace butl
{
  template <typename C>
  inline std::pair<std::basic_string<C>, bool>
  regex_replace_search (const std::basic_string<C>& s,
                        const std::basic_regex<C>& re,
                        const std::basic_string<C>& fmt,
                        std::regex_constants::match_flag_type flags)
  {
    using namespace std;

    using it = typename basic_string<C>::const_iterator;

    basic_string<C> r;
    bool match (regex_replace_search (s, re, fmt,
                                      [&r] (it b, it e) {r.append (b, e);},
                                      flags));

    return make_pair (move (r), match);
  }

  template <typename C>
  inline std::pair<std::basic_regex<C>, std::basic_string<C>>
  regex_replace_parse (const std::basic_string<C>& s,
                       std::regex_constants::syntax_option_type f)
  {
    return regex_replace_parse (s.c_str (), s.size (), f);
  }

  template <typename C>
  inline std::pair<std::basic_regex<C>, std::basic_string<C>>
  regex_replace_parse (const C* s,
                       std::regex_constants::syntax_option_type f)
  {
    return regex_replace_parse (
      s, std::basic_string<C>::traits_type::length (s), f);
  }

  template <typename C>
  inline std::basic_string<C>
  regex_replace_match_results (
    const std::match_results<typename std::basic_string<C>::const_iterator>& m,
    const std::basic_string<C>& fmt)
  {
    return regex_replace_match_results (m, fmt.c_str (), fmt.size ());
  }
}
