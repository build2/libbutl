namespace butl
{
  namespace regex
  {
    template <typename C>
    inline std::pair<typename regex_char_traits<C>::string_type, bool>
    regex_replace_search (const typename regex_char_traits<C>::string_type& s,
                          const basic_regex<C>& re,
                          const typename regex_char_traits<C>::string_type& fmt,
                          regex_constants::match_flag_type flags)
    {
      using string_type = typename regex_char_traits<C>::string_type;
      using it          = typename string_type::const_iterator;

      string_type r;
      bool match (regex_replace_search (s, re, fmt,
                                        [&r] (it b, it e) {r.append (b, e);},
                                        flags));

      return std::make_pair (std::move (r), match);
    }

    template <typename C>
    inline std::pair<basic_regex<C>, typename regex_char_traits<C>::string_type>
    regex_replace_parse (const typename regex_char_traits<C>::string_type& s,
                         regex_constants::syntax_option_type f)
    {
      return regex_replace_parse (s.c_str (), s.size (), f);
    }

    template <typename C>
    inline std::pair<basic_regex<C>, typename regex_char_traits<C>::string_type>
    regex_replace_parse (const C* s,
                         regex_constants::syntax_option_type f)
    {
      return regex_replace_parse (s, regex_char_traits<C>::length (s), f);
    }

    template <typename I, typename C>
    inline typename regex_char_traits<C>::string_type
    regex_replace_match_results (
      const match_results<I>& m,
      const typename regex_char_traits<C>::string_type& fmt)
    {
      return regex_replace_match_results (m, fmt.c_str (), fmt.size ());
    }
  }
}
