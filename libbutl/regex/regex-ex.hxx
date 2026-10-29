#pragma once

#include <utility>  // pair, make_pair(), move()
#include <cstddef>  // size_t
#include <iterator> // iterator_traits

#include <libstud/regex/regex.hxx>

namespace stud
{
  namespace regex
  {
    // The regex semantics for the following functions is like that of
    // std::regex_replace() extended the standard ECMA-262 substitution escape
    // sequences with a subset of Perl sequences:
    //
    // \\, \n, \u, \l, \U, \L, \E, \1, ..., \9
    //
    // Notes and limitations:
    //
    // - The only valid regex_constants flags are match_default,
    //   format_first_only and format_no_copy.
    //
    // - If backslash doesn't start any of the listed sequences then it is
    //   silently dropped and the following character is copied as is.
    //
    // - The character case conversion is performed according to the global
    //   C++ locale (which is, unless changed, is the same as C locale and
    //   both default to the POSIX locale aka "C"). This, in particular, means
    //   that for character types other than char, the \u, \l, \U, \L, \E
    //   sequences are effectively not supported.
    //

    // Call specified append() function for non-matched substrings and matched
    // substring replacements returning true if search succeeded. The function
    // must be callable with the following signature:
    //
    // void
    // append(regex_char_traits<C>::string_type::iterator begin,
    //        regex_char_traits<C>::string_type::iterator end);
    //
    template <typename C, typename F>
    bool
    regex_replace_search (const typename regex_char_traits<C>::string_type&,
                          const basic_regex<C>&,
                          const typename regex_char_traits<C>::string_type& fmt,
                          F&& append,
                          regex_constants::match_flag_type =
                            regex_constants::match_default);

    // As above but concatenate non-matched substrings and matched substring
    // replacements into a string returning it as well as whether the search
    // succeeded.
    //
    template <typename C>
    std::pair<typename regex_char_traits<C>::string_type, bool>
    regex_replace_search (const typename regex_char_traits<C>::string_type&,
                          const basic_regex<C>&,
                          const typename regex_char_traits<C>::string_type& fmt,
                          regex_constants::match_flag_type =
                            regex_constants::match_default);

    // Match the entire string and, if it matches, return the string
    // replacement.
    //
    template <typename C>
    std::pair<typename regex_char_traits<C>::string_type, bool>
    regex_replace_match (const typename regex_char_traits<C>::string_type&,
                         const basic_regex<C>&,
                         const typename regex_char_traits<C>::string_type& fmt);

    // As above but using match_results.
    //
    template <typename I,
              typename C = typename std::iterator_traits<I>::value_type>
    typename regex_char_traits<C>::string_type
    regex_replace_match_results (
      const match_results<I>&,
      const typename regex_char_traits<C>::string_type& fmt);

    template <typename I,
              typename C = typename std::iterator_traits<I>::value_type>
    typename regex_char_traits<C>::string_type
    regex_replace_match_results (const match_results<I>&,
                                 const C* fmt, std::size_t fmt_n);

    // Parse the '/<regex>/<format>/' replacement string into the regex/format
    // pair. Other character can be used as a delimiter instead of '/'. Throw
    // std::invalid_argument or stud::regex::regex_error on parsing error.
    //
    // Note: escaping of the delimiter character is not (yet) supported.
    //
    template <typename C>
    std::pair<basic_regex<C>, typename regex_char_traits<C>::string_type>
    regex_replace_parse (const typename regex_char_traits<C>::string_type&,
                         regex_constants::syntax_option_type =
                           regex_constants::ECMAScript);

    template <typename C>
    std::pair<basic_regex<C>, typename regex_char_traits<C>::string_type>
    regex_replace_parse (const C*,
                         regex_constants::syntax_option_type =
                           regex_constants::ECMAScript);

    template <typename C>
    std::pair<basic_regex<C>, typename regex_char_traits<C>::string_type>
    regex_replace_parse (const C*, std::size_t,
                         regex_constants::syntax_option_type =
                           regex_constants::ECMAScript);

    // As above but return string instead of regex and do not fail if there is
    // text after the last delimiter instead returning its position.
    //
    template <typename C>
    std::pair<typename regex_char_traits<C>::string_type,
              typename regex_char_traits<C>::string_type>
    regex_replace_parse (const C*, std::size_t, std::size_t& end);
  }
}

#include <libstud/regex/regex-ex.ixx>
#include <libstud/regex/regex-ex.txx>
