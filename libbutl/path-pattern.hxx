// file      : libbutl/path-pattern.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <cassert>
#include <cstdint>  // uint16_t
#include <cstddef>  // ptrdiff_t, size_t
#include <iterator> // input_iterator_tag

#include <libbutl/path.hxx>
#include <libbutl/optional.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // Wildcard pattern match (aka glob).
  //
  // The wildcard pattern contains the literal characters that match
  // themselves and the wildcard characters that match a single or multiple
  // characters. Currently the following wildcards are supported:
  //
  // *     - match any number of characters (including zero)
  // ?     - match any single character
  // [...] - match a character with a "bracket expression"; currently we only
  //         support literal characters and ranges (no character/equivalence
  //         classes, etc; see Pattern Matching Notation section of the Shell
  //         Command Language POSIX specification for details)
  //
  // Note also that currently we don't support the special characters
  // backslash-escaping (as mandated by POSIX).

  // Path match/search flags.
  //
  enum class path_match_flags: std::uint16_t
  {
    // Follow symlinks. This only applies to symlinks that are matched against
    // the rightmost component of the pattern. In particular, this mean that
    // such symlinks will never match a directory pattern and some results can
    // be missing for the recursive rightmost component.
    //
    // Note that this flag is only used for path_search().
    //
    follow_symlinks = 0x1,

    // Make wildcard-only pattern component (e.g., `*/...`, `.../*/...`, or
    // `.../*`) match absent path component. For example, with this flag
    // set, the `a/*/b` pattern matches not only `a/x/b` path, but also `a/b`.
    //
    // Note that this does not apply to single-component patterns and the
    // pattern type is always preserved. In particular, the `a/*/` pattern
    // matches `a/` but not `a`.
    //
    // Finally, keep in mind that only absent directory components can be
    // matched this way. In particular, pattern `a*/*` does not match `ab`
    // (but `a*/*/` matches `ab/`).
    //
    match_absent = 0x2,

    none = 0
  };

  inline path_match_flags operator& (path_match_flags, path_match_flags);
  inline path_match_flags operator| (path_match_flags, path_match_flags);
  inline path_match_flags operator&= (path_match_flags&, path_match_flags);
  inline path_match_flags operator|= (path_match_flags&, path_match_flags);

  // Return true if name matches pattern. Both must be single path components,
  // possibly with a trailing directory separator to indicate a directory.
  //
  // If the pattern ends with a directory separator, then it only matches a
  // directory name (i.e., ends with a directory separator, but potentially
  // different). Otherwise, it only matches a non-directory name (no trailing
  // directory separator).
  //
  LIBBUTL_SYMEXPORT bool
  path_match (const std::string& name, const std::string& pattern);

  // Return true if path entry matches pattern. Note that the match is
  // performed literally, with no paths normalization being performed. The
  // start directory is used if the first pattern component is a self-matching
  // wildcard (see below for the start directory and wildcard semantics).
  //
  // In addition to the wildcard characters, it also recognizes the ** and ***
  // wildcard sequences (see path_search() for details).
  //
  LIBBUTL_SYMEXPORT bool
  path_match (const path& entry,
              const path& pattern,
              const dir_path& start = dir_path (),
              path_match_flags = path_match_flags::none);

  // Return true if a name contains the wildcard characters.
  //
  bool
  path_pattern (const std::string&);

  // Return true if a name contains the ** wildcard sequences.
  //
  bool
  path_pattern_recursive (const std::string&);

  // Return true if a name contains the *** wildcard sequences.
  //
  bool
  path_pattern_self_matching (const std::string&);

  // Return true if a path contains the pattern components.
  //
  bool
  path_pattern (const path&);

  // Return the number of recursive pattern components.
  //
  // Knowing the number of such components allows us to make some assumptions
  // regarding the search result. For example, if it is zero or one, then the
  // result contains no duplicates.
  //
  // Also note that the result can be used as bool.
  //
  std::size_t
  path_pattern_recursive (const path&);

  // Return true if the path is not empty and its first component is a self-
  // matching pattern.
  //
  bool
  path_pattern_self_matching (const path&);

  // Iteration over pattern terminals.
  //
  enum class path_pattern_term_type
  {
    literal,  // Literal character.
    question, // Question mark wildcard.
    star,     // Star wildcard.
    bracket   // Bracket expression wildcard.
  };

  class path_pattern_term
  {
  public:
    path_pattern_term_type      type;
    std::string::const_iterator begin;
    std::string::const_iterator end;

    std::size_t
    size () const {return end - begin;}

    // Predicates.
    //
    bool literal  () const {return type == path_pattern_term_type::literal;}
    bool question () const {return type == path_pattern_term_type::question;}
    bool star     () const {return type == path_pattern_term_type::star;}
    bool bracket  () const {return type == path_pattern_term_type::bracket;}
  };

  // Return the literal terminal character.
  //
  char
  get_literal (const path_pattern_term&);

  // Match a character against the bracket expression terminal.
  //
  LIBBUTL_SYMEXPORT bool
  match_bracket (char, const path_pattern_term&);

  class LIBBUTL_SYMEXPORT path_pattern_iterator
  {
  public:
    using value_type = path_pattern_term;
    using pointer = const path_pattern_term*;
    using reference = const path_pattern_term&;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::input_iterator_tag;

    explicit
    path_pattern_iterator (const std::string&);

    path_pattern_iterator (std::string::const_iterator begin,
                           std::string::const_iterator end);

    path_pattern_iterator () = default; // Create the end iterator.

    path_pattern_iterator& operator++ () {assert (t_); next (); return *this;}

    reference operator*  () const {assert (t_); return *t_;}
    pointer   operator-> () const {assert (t_); return &*t_;}

    friend bool
    operator== (const path_pattern_iterator&, const path_pattern_iterator&);

    friend bool
    operator!= (const path_pattern_iterator&, const path_pattern_iterator&);

  private:
    void
    next ();

  private:
    // nullopt denotes the end iterator.
    //
    // Note that the default-constructed i_ and e_ iterators (having singular
    // values) may not represent the end iterator as are not comparable for
    // equality. That's why we use an absent term to represent such an
    // iterator.
    //
    optional<path_pattern_term> t_;

    std::string::const_iterator i_;
    std::string::const_iterator e_;
  };

  // Range-based for loop support.
  //
  // for (const path_pattern_term& t: path_pattern_iterator (pattern)) ...
  //
  path_pattern_iterator begin (const path_pattern_iterator&);
  path_pattern_iterator end   (const path_pattern_iterator&);
}

#include <libbutl/path-pattern.ixx>
