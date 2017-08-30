// file      : libbutl/regex.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_REGEX_HXX
#define LIBBUTL_REGEX_HXX

#include <regex>
#include <iosfwd>
#include <string>  // basic_string
#include <utility> // pair

#include <libbutl/export.hxx>

namespace butl
{
  // Call specified append() function for non-matched substrings and matched
  // substring replacements returning true if search succeeded. The function
  // must be callable with the following signature:
  //
  // void
  // append(basic_string<C>::iterator begin, basic_string<C>::iterator end);
  //
  // The regex semantics is like that of std::regex_replace() extended the
  // standard ECMA-262 substitution escape sequences with a subset of Perl
  // sequences:
  //
  // \\, \u, \l, \U, \L, \E, \1, ..., \9
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
  template <typename C, typename F>
  bool
  regex_replace_ex (const std::basic_string<C>&,
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
  regex_replace_ex (const std::basic_string<C>&,
                    const std::basic_regex<C>&,
                    const std::basic_string<C>& fmt,
                    std::regex_constants::match_flag_type =
                      std::regex_constants::match_default);
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

#endif // LIBBUTL_REGEX_HXX
