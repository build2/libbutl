// file      : libbutl/regex.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <regex>
#include <iosfwd>
#include <string>
#include <utility> // pair
#include <cstddef> // size_t

#if defined(__clang__)
#  if __has_include(<__config>)
#    include <__config>          // _LIBCPP_VERSION
#  endif
#endif

#include <libbutl/export.hxx>

namespace butl
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
  //   both default to the POSIX locale aka "C").
  //

  // Call specified append() function for non-matched substrings and matched
  // substring replacements returning true if search succeeded. The function
  // must be callable with the following signature:
  //
  // void
  // append(basic_string<C>::iterator begin, basic_string<C>::iterator end);
  //
  template <typename C, typename F>
  bool
  regex_replace_search (const std::basic_string<C>&,
                        const std::basic_regex<C>&,
                        const std::basic_string<C>& fmt,
                        F&& append,
                        std::regex_constants::match_flag_type =
                        std::regex_constants::match_default);

  // As above but concatenate non-matched substrings and matched substring
  // replacements into a string returning it as well as whether the search
  // succeeded.
  //
  template <typename C>
  std::pair<std::basic_string<C>, bool>
  regex_replace_search (const std::basic_string<C>&,
                        const std::basic_regex<C>&,
                        const std::basic_string<C>& fmt,
                        std::regex_constants::match_flag_type =
                          std::regex_constants::match_default);

  // Match the entire string and, if it matches, return the string replacement.
  //
  template <typename C>
  std::pair<std::basic_string<C>, bool>
  regex_replace_match (const std::basic_string<C>&,
                       const std::basic_regex<C>&,
                       const std::basic_string<C>& fmt);

  // As above but using match_results.
  //
  template <typename C>
  std::basic_string<C>
  regex_replace_match_results (
    const std::match_results<typename std::basic_string<C>::const_iterator>&,
    const std::basic_string<C>& fmt);

  template <typename C>
  std::basic_string<C>
  regex_replace_match_results (
    const std::match_results<typename std::basic_string<C>::const_iterator>&,
    const C* fmt, std::size_t fmt_n);

  // Parse the '/<regex>/<format>/' replacement string into the regex/format
  // pair. Other character can be used as a delimiter instead of '/'. Throw
  // std::invalid_argument or std::regex_error on parsing error.
  //
  // Note: escaping of the delimiter character is not (yet) supported.
  //
  template <typename C>
  std::pair<std::basic_regex<C>, std::basic_string<C>>
  regex_replace_parse (const std::basic_string<C>&,
                       std::regex_constants::syntax_option_type =
                         std::regex_constants::ECMAScript);

  template <typename C>
  std::pair<std::basic_regex<C>, std::basic_string<C>>
  regex_replace_parse (const C*,
                       std::regex_constants::syntax_option_type =
                         std::regex_constants::ECMAScript);

  template <typename C>
  std::pair<std::basic_regex<C>, std::basic_string<C>>
  regex_replace_parse (const C*, size_t,
                       std::regex_constants::syntax_option_type =
                         std::regex_constants::ECMAScript);

  // As above but return string instead of regex and do not fail if there is
  // text after the last delimiter instead returning its position.
  //
  template <typename C>
  std::pair<std::basic_string<C>, std::basic_string<C>>
  regex_replace_parse (const C*, size_t, size_t& end);
}

namespace std
{
  // Print regex error description but only if it is meaningful (this is also
  // why we have to print leading colon).
  //
  LIBBUTL_SYMEXPORT ostream&
  operator<< (ostream&, const regex_error&);
}

#include <libbutl/regex.ixx>
#include <libbutl/regex.txx>
