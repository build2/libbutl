// file      : libbutl/regex.txx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <locale>
#include <stdexcept> // invalid_argument

namespace butl
{
  template <typename C>
  std::basic_string<C>
  regex_replace_match_results (
    const std::match_results<typename std::basic_string<C>::const_iterator>& m,
    const C* fmt, std::size_t n)
  {
    using namespace std;

    using string_type = basic_string<C>;
    using str_it      = typename string_type::const_iterator;

    string_type r;

    // Note that we are using char type literals with the assumption that
    // being ASCII characters they will be properly "widened" to the
    // corresponding literals of the C template parameter type.
    //
    auto digit = [] (C c) -> int
    {
      return c >= '0' && c <= '9' ? c - '0' : -1;
    };

    enum class case_conv {none, upper, lower, upper_once, lower_once}
    mode (case_conv::none);

    locale cl; // Copy of the global C++ locale.

    auto conv_chr = [&mode, &cl] (C c) -> C
    {
      switch (mode)
      {
      case case_conv::upper_once: mode = case_conv::none; // Fall through.
      case case_conv::upper:      c = toupper (c, cl); break;
      case case_conv::lower_once: mode = case_conv::none; // Fall through.
      case case_conv::lower:      c = tolower (c, cl); break;
      case case_conv::none:       break;
      }
      return c;
    };

    auto append_chr = [&r, &conv_chr] (C c) {r.push_back (conv_chr (c));};

    auto append_str = [&r, &mode, &conv_chr] (str_it b, str_it e)
    {
      // Optimize for the common case.
      //
      if (mode == case_conv::none)
        r.append (b, e);
      else
      {
        for (str_it i (b); i != e; ++i)
          r.push_back (conv_chr (*i));
      }
    };

    for (size_t i (0); i < n; ++i)
    {
      C c (fmt[i]);

      switch (c)
      {
      case '$':
        {
          // Check if this is a $-based escape sequence. Interpret it
          // accordingly if that's the case, treat '$' as a regular character
          // otherwise.
          //
          c = fmt[++i]; // '\0' if last.

          switch (c)
          {
          case '$': append_chr (c); break;
          case '&': append_str (m[0].first, m[0].second); break;
          case '`':
            {
              append_str (m.prefix ().first, m.prefix ().second);
              break;
            }
          case '\'':
            {
              append_str (m.suffix ().first, m.suffix ().second);
              break;
            }
          default:
            {
              // Check if this is a sub-expression 1-based index ($n or $nn).
              // Append the matching substring if that's the case. Treat '$'
              // as a regular character otherwise. Index greater than the
              // sub-expression count is silently ignored.
              //
              int si (digit (c));
              if (si >= 0)
              {
                int d;
                if ((d = digit (fmt[i + 1])) >= 0) // '\0' if last.
                {
                  si = si * 10 + d;
                  ++i;
                }
              }

              if (si > 0)
              {
                // m[0] refers to the matched substring. Note that we ignore
                // unmatched sub-expression references.
                //
                if (static_cast<size_t> (si) < m.size () && m[si].matched)
                  append_str (m[si].first, m[si].second);
              }
              else
              {
                // Not a $-based escape sequence so treat '$' as a regular
                // character.
                //
                --i;
                append_chr ('$');
              }

              break;
            }
          }

          break;
        }
      case '\\':
        {
          c = fmt[++i]; // '\0' if last.

          switch (c)
          {
          case '\\': append_chr (c);    break;
          case 'n':  append_chr ('\n'); break;

          case 'u': mode = case_conv::upper_once; break;
          case 'l': mode = case_conv::lower_once; break;
          case 'U': mode = case_conv::upper;      break;
          case 'L': mode = case_conv::lower;      break;
          case 'E': mode = case_conv::none;       break;
          default:
            {
              // Check if this is a sub-expression 1-based index. Append the
              // matching substring if that's the case, Skip '\\' otherwise.
              // Index greater than the sub-expression count is silently
              // ignored.
              //
              int si (digit (c));
              if (si > 0)
              {
                // m[0] refers to the matched substring. Note that we ignore
                // unmatched sub-expression references.
                //
                if (static_cast<size_t> (si) < m.size () && m[si].matched)
                  append_str (m[si].first, m[si].second);
              }
              else
                --i;

              break;
            }
          }

          break;
        }
      default:
        {
          // Append a regular character.
          //
          append_chr (c);
          break;
        }
      }
    }

    return r;
  }

  template <typename C>
  std::pair<std::basic_string<C>, bool>
  regex_replace_match (const std::basic_string<C>& s,
                       const std::basic_regex<C>& re,
                       const std::basic_string<C>& fmt)
  {
    using namespace std;

    using string_type = basic_string<C>;
    using str_it      = typename string_type::const_iterator;

    match_results<str_it> m;
    bool match (regex_match (s, m, re));

    return make_pair (match ? regex_replace_match_results (m, fmt) : string (),
                      match);
  }

  template <typename C, typename F>
  bool
  regex_replace_search (const std::basic_string<C>& s,
                        const std::basic_regex<C>& re,
                        const std::basic_string<C>& fmt,
                        F&& append,
                        std::regex_constants::match_flag_type flags)
  {
    using namespace std;

    using string_type = basic_string<C>;
    using str_it      = typename string_type::const_iterator;
    using regex_it    = regex_iterator<str_it>;

    bool first_only ((flags & regex_constants::format_first_only) != 0);
    bool no_copy ((flags & regex_constants::format_no_copy) != 0);

    // Beginning of the last unmatched substring.
    //
    str_it ub (s.begin ());

    regex_it b (s.begin (), s.end (), re, flags);
    regex_it e;
    bool match (b != e);

    // For libc++, the end-of-sequence regex iterator can never be reached
    // for some regular expressions (LLVM bug #33681). We will check if the
    // matching sequence start is the same as the one for the previous match
    // and bail out if that's the case.
    //
#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION <= 4000
    str_it pm;
#endif

    for (regex_it i (b); i != e; ++i)
    {
      const match_results<str_it>& m (*i);

#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION <= 4000
      if (i != b && m[0].first == pm)
        break;

      pm = m[0].first;
#endif

      // Copy the preceeding unmatched substring, save the beginning of the
      // one that follows.
      //
      if (!no_copy)
      {
        append (ub, m.prefix ().second);
        ub = m.suffix ().first;
      }

      if (first_only && i != b)
      {
        // Append matched substring.
        //
        if (!no_copy)
          append (m[0].first, m[0].second);
      }
      else
      {
        // The standard implementation calls m.format() here. We perform our
        // own formatting.
        //
        string_type r (regex_replace_match_results (m, fmt));
        append (r.begin (), r.end ());
      }
    }

    // Append the rightmost non-matched substring.
    //
    if (!no_copy)
      append (ub, s.end ());

    return match;
  }

  template <typename C>
  std::pair<std::basic_regex<C>, std::basic_string<C>>
  regex_replace_parse (const C* s, size_t n,
                       std::regex_constants::syntax_option_type f)
  {
    using namespace std;

    using string_type = basic_string<C>;

    size_t e;
    pair<string_type, string_type> r (regex_replace_parse (s, n, e));

    if (e != n)
      throw invalid_argument ("junk after trailing delimiter");

    return make_pair (basic_regex<C> (r.first, f), move (r.second));
  }

  template <typename C>
  std::pair<std::basic_string<C>, std::basic_string<C>>
  regex_replace_parse (const C* s, size_t n, size_t& e)
  {
    using namespace std;

    using string_type = basic_string<C>;

    if (n == 0)
      throw invalid_argument ("no leading delimiter");

    const C* b (s); // Save the beginning of the string.

    char delim (s[0]);

    // Position to the regex first character and find the regex-terminating
    // delimiter.
    //
    --n;
    ++s;

    const C* p (string_type::traits_type::find (s, n, delim));

    if (p == nullptr)
      throw invalid_argument ("no delimiter after regex");

    // Empty regex matches nothing, so not of much use.
    //
    if (p == s)
      throw invalid_argument ("empty regex");

    // Save the regex.
    //
    string_type re (s, p - s);

    // Position to the format first character and find the trailing delimiter.
    //
    n -= p - s + 1;
    s  = p + 1;

    p = string_type::traits_type::find (s, n, delim);

    if (p == nullptr)
      throw invalid_argument ("no delimiter after replacement");

    e = p - b + 1;
    return make_pair (move (re), string_type (s, p - s));
  }
}
