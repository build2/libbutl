// file      : libbutl/regex.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

LIBBUTL_MODEXPORT namespace butl //@@ MOD Clang needs this for some reason.
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
}
