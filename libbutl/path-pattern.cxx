// file      : libbutl/path-pattern.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/path-pattern.hxx>

#include <cassert>
#include <iterator> // reverse_iterator
#include <algorithm> // find()

#include <libbutl/utility.hxx>    // lcase()[_WIN32]
#include <libbutl/filesystem.hxx> // path_search()

using namespace std;

namespace butl
{
  // patterns
  //
  static inline bool
  match (char c, char pc)
  {
#ifndef _WIN32
    return c == pc;
#else
    return lcase (c) == lcase (pc);
#endif
  }

  bool
  match_bracket (char c, const path_pattern_term& pt)
  {
    using iterator = string::const_iterator;

    assert (pt.bracket ());

    iterator i (pt.begin + 1); // Position after '['.
    iterator e (pt.end   - 1); // Position at ']'.

    bool invert (*i == '!');
    if (invert)
      ++i;

    bool r (false);
    for (iterator b (i); i != e && !r; ++i)
    {
      char bc (*i);

      // If '-' is a first or last character in the bracket expression then
      // match it literally and match the range otherwise.
      //
      if (bc == '-' && i != b && i + 1 != e) // Match the range?
      {
        // Note that we have already matched the range left endpoint character
        // unsuccessfully (otherwise we wouldn't be here), so now we test if
        // the character belongs to the (min-char, max-char] range.
        //
        // Also note that on Windows we match case insensitively and so can't
        // just compare the character with the range endpoints. Thus, we
        // fallback to matching each range character individually.
        //
#ifndef _WIN32
        r = c > *(i - 1) && c <= *(i + 1);
#else
        for (char bc (*(i - 1) + 1), mx (*(i + 1)); bc <= mx && !r; ++bc)
          r = match (c, bc);
#endif

        ++i; // Position to the range max character.
      }
      else // Match against the expression character literally.
        r = match (c, bc);
    }

    return r != invert;
  }

  // Match the name [ni, ne) to the pattern [pi, pe) that may not contain
  // bracket expressions. Ranges can be empty.
  //
  static bool
  match_no_brackets (string::const_iterator pi, string::const_iterator pe,
                     string::const_iterator ni, string::const_iterator ne)
  {
    using reverse_iterator = std::reverse_iterator<string::const_iterator>;

    reverse_iterator rpi (pe);
    reverse_iterator rpe (pi);

    reverse_iterator rni (ne);
    reverse_iterator rne (ni);

    // Match the pattern suffix (follows the last *) to the name trailing
    // characters.
    //
    char pc ('\0');
    for (; rpi != rpe && (pc = *rpi) != '*' && rni != rne; ++rpi, ++rni)
    {
      if (!match (*rni, pc) && pc != '?')
        return false;
    }

    // If we got to the (reversed) end of the pattern (no * is encountered)
    // than we are done. The success depends on if we got to the (reversed) end
    // of the name as well.
    //
    if (rpi == rpe)
      return rni == rne;

    // If we didn't reach * in the pattern then we reached the (reversed) end
    // of the name. That means we have unmatched non-star terms in the
    // pattern, and so match failed.
    //
    if (pc != '*')
    {
      assert (rni == rne);
      return false;
    }

    // Match the pattern prefix (ends with the first *) to the name leading
    // characters. If they mismatch we failed. Otherwise if this is an only *
    // in the pattern (matches whatever is left in the name) then we succeed,
    // otherwise we perform backtracking (recursively).
    //
    pe = rpi.base ();
    ne = rni.base ();

    // Compare the pattern and the name term by char until the name suffix or
    // * is encountered in the pattern (whichever happens first). Fail if a
    // char mismatches.
    //
    for (; (pc = *pi) != '*' && ni != ne; ++pi, ++ni)
    {
      if (!match (*ni, pc) && pc != '?')
        return false;
    }

    // If we didn't get to * in the pattern then we got to the name suffix.
    // That means that the pattern has unmatched non-star terms, and so match
    // failed.
    //
    if (pc != '*')
    {
      assert (ni == ne);
      return false;
    }

    // If * that we have reached is the last one, then it matches whatever is
    // left in the name (including an empty range).
    //
    if (++pi == pe)
      return true;

    // Perform backtracking.
    //
    // From now on, we will call the pattern not-yet-matched part (starting
    // the leftmost * and ending the rightmost one inclusively) as pattern, and
    // the name not-yet-matched part as name.
    //
    // Here we sequentially assume that * that starts the pattern matches the
    // name leading part (staring from an empty one and iterating till the full
    // name). So if, at some iteration, the pattern trailing part (that follows
    // the leftmost *) matches the name trailing part, then the pattern matches
    // the name.
    //
    bool r;
    for (; !(r = match_no_brackets (pi, pe, ni, ne)) && ni != ne; ++ni) ;
    return r;
  }

  // Match a character against the pattern term.
  //
  static inline bool
  match (char c, const path_pattern_term& pt)
  {
    switch (pt.type)
    {
      // Matches any character.
      //
    case path_pattern_term_type::star:
    case path_pattern_term_type::question: return true;

    case path_pattern_term_type::bracket:
      {
        return match_bracket (c, pt);
      }

    case path_pattern_term_type::literal:
      {
        return match (c, get_literal (pt));
      }
    }

    assert (false); // Can't be here.
    return false;
  }

  // Match the name [ni, ne) to the pattern [pi, pe). Ranges can be empty.
  //
  static bool
  match (string::const_iterator pi, string::const_iterator pe,
         string::const_iterator ni, string::const_iterator ne)
  {
    // If the pattern doesn't contain the bracket expressions then reduce to
    // the "clever" approach (see the implementation notes below for details).
    //
    if (find (pi, pe, '[') == pe)
      return match_no_brackets (pi, pe, ni, ne);

    // Match the pattern prefix (precedes the first *) to the name leading
    // characters.
    //
    path_pattern_iterator ppi (pi, pe);
    path_pattern_iterator ppe;
    path_pattern_term pt;

    for (; ppi != ppe && !(pt = *ppi).star () && ni != ne; ++ppi, ++ni)
    {
      if (!match (*ni, pt))
        return false;
    }

    // If we got to the end of the pattern (no * is encountered) than we are
    // done. The success depends on if we got to the end of the name as well.
    //
    if (ppi == ppe)
      return ni == ne;

    // If we didn't reach * in the pattern then we reached the end of the
    // name. That means we have unmatched non-star terms in the pattern, and
    // so match failed.
    //
    if (!pt.star ())
    {
      assert (ni == ne);
      return false;
    }

    // If * that we have reached is the last term, then it matches whatever is
    // left in the name (including an empty range).
    //
    if (++ppi == ppe)
      return true;

    // Switch back to the string iterator and perform backtracking.
    //
    // From now on, we will call the pattern not-yet-matched part (starting
    // the leftmost *) as pattern, and the name not-yet-matched part as name.
    //
    // Here we sequentially assume that * that starts the pattern matches the
    // name leading part (see match_no_brackets() for details).
    //
    bool r;
    for (pi = ppi->begin; !(r = match (pi, pe, ni, ne)) && ni != ne; ++ni) ;
    return r;
  }

  bool
  path_match (const string& name, const string& pattern)
  {
    // Implementation notes:
    //
    // - This has a good potential of becoming hairy quickly so need to strive
    //   for an elegant way to implement this.
    //
    // - Most patterns will contains a single * wildcard with a prefix and/or
    //   suffix (e.g., *.txt, foo*, f*.txt). Something like this is not very
    //   common: *foo*.
    //
    //   So it would be nice to have a clever implementation that first
    //   "anchors" itself with a literal prefix and/or suffix and only then
    //   continue with backtracking. In other words, reduce:
    //
    //   *.txt  vs foo.txt -> * vs foo
    //   foo*   vs foo.txt -> * vs .txt
    //   f*.txt vs foo.txt -> * vs oo
    //
    //   Note that this approach fails if the pattern may contain bracket
    //   expressions. You can't easily recognize a suffix scanning backwards
    //   since * semantics depends on the characters to the left:
    //
    //   f[o*]o - * is not a wildcard
    //    fo*]o - * is a wildcard
    //
    //   That's why we will start with the straightforward left-to-right
    //   matching and reduce to the "clever" approach when the remaining part
    //   of the pattern doesn't contain bracket expressions.

    auto pi (pattern.rbegin ());
    auto pe (pattern.rend ());

    auto ni (name.rbegin ());
    auto ne (name.rend ());

    // The name doesn't match the pattern if it is of a different type than the
    // pattern is.
    //
    bool pd (pi != pe && path::traits_type::is_separator (*pi));
    bool nd (ni != ne && path::traits_type::is_separator (*ni));

    if (pd != nd)
      return false;

    // Skip trailing separators if present.
    //
    if (pd)
    {
      ++pi;
      ++ni;
    }

    return match (pattern.begin (), pi.base (), name.begin (), ni.base ());
  }

  bool
  path_match (const path& entry,
              const path& pattern,
              const dir_path& start,
              path_match_flags flags)
  {
    bool r (false);

    auto match = [&entry, &r] (path&& p, const string&, bool interim)
    {
      // If we found the entry (possibly through one of the recursive
      // components) no need to search further.
      //
      if (p == entry && !interim)
      {
        r = true;
        return false;
      }

      return true;
    };

    path_search (pattern, entry, match, start, flags);
    return r;
  }

  // path_pattern_iterator
  //
  void path_pattern_iterator::
  next ()
  {
    if (i_ == e_)
    {
      t_ = nullopt; // Convert the object into the end iterator.
      return;
    }

    auto next = [this] (path_pattern_term_type t)
    {
      assert (t != path_pattern_term_type::bracket);

      t_ = path_pattern_term {t, i_, i_ + 1};
      ++i_;
    };

    switch (*i_)
    {
    case '?':
      {
        next (path_pattern_term_type::question);
        break;
      }
    case '*':
      {
        next (path_pattern_term_type::star);
        break;
      }
    case '[':
      {
        // Try to find the bracket expression end.
        //
        // Note that '[' doesn't necessarily starts the bracket expression (no
        // closing bracket, empty, etc). If that's the case, then we end up
        // with the '[' literal terminal.
        //
        bool expr (false);
        for (;;) // Breakout loop.
        {
          string::const_iterator i (i_ + 1); // Position after '['.

          if (i == e_) // Is '[' the pattern last character?
            break;

          bool invert (*i == '!');
          if (invert && ++i == e_) // Is '!' the pattern last character?
            break;

          // Find the bracket expression end.
          //
          // Note that the bracket expression may not be empty and ']' is a
          // literal if it is the first expression character.
          //
          for (++i; i != e_ && *i != ']'; ++i) ;

          if (i == e_) // The closing bracket is not found?
            break;

          expr = true;

          ++i; // Position after ']'.

          t_ = path_pattern_term {path_pattern_term_type::bracket, i_, i};

          i_ = i;
          break;
        }

        // Fallback to '[' literal if it is not a bracket expression.
        //
        if (expr)
          break;
      }
      // Fall through.
    default:
      {
        next (path_pattern_term_type::literal);
      }
    }
  }
}
