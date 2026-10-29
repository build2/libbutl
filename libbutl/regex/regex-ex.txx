#include <locale>
#include <cctype>    // toupper(), tolower()
#include <stdexcept> // invalid_argument

namespace butl
{
  namespace regex
  {
    template <typename I, typename C>
    typename regex_char_traits<C>::string_type
    regex_replace_match_results (const match_results<I>& m,
                                 const C* fmt, std::size_t n)
    {
      using std::size_t;

      using string_type = typename regex_char_traits<C>::string_type;
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

      // Note that to properly support case conversions, we should probably
      // use the tolower() and alike functions of the ctype facet retrieved
      // from an object of regex_traits<C>::locale_type type (for example, by
      // using tolower(CharT,locale) template function). Let's, however, keep
      // it simple for now.
      //
      std::locale cl; // Copy of the global C++ locale.

      auto conv_chr = [&mode, &cl] (C c) -> C
      {
        switch (mode)
        {
        case case_conv::upper_once: mode = case_conv::none; // Fall through.
        case case_conv::upper:      c = std::toupper (c, cl); break;
        case case_conv::lower_once: mode = case_conv::none; // Fall through.
        case case_conv::lower:      c = std::tolower (c, cl); break;
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
        {
          r.append (b, e);
        }
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
            // accordingly if that's the case, treat '$' as a regular
            // character otherwise.
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
                // Check if this is a sub-expression 1-based index ($n or
                // $nn). Append the matching substring if that's the case.
                // Treat '$' as a regular character otherwise. Index greater
                // than the sub-expression count is silently ignored.
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
    std::pair<typename regex_char_traits<C>::string_type, bool>
    regex_replace_match (const typename regex_char_traits<C>::string_type& s,
                         const basic_regex<C>& re,
                         const typename regex_char_traits<C>::string_type& fmt)
    {
      using string_type = typename regex_char_traits<C>::string_type;
      using str_it      = typename string_type::const_iterator;

      match_results<str_it> m;
      bool match (regex_match (s, m, re));

      return std::make_pair ((match
                              ? regex_replace_match_results (m, fmt)
                              : string_type ()),
                             match);
    }

    template <typename C, typename F>
    bool
    regex_replace_search (const typename regex_char_traits<C>::string_type& s,
                          const basic_regex<C>& re,
                          const typename regex_char_traits<C>::string_type& fmt,
                          F&& append,
                          regex_constants::match_flag_type flags)
    {
      using string_type = typename regex_char_traits<C>::string_type;
      using str_it      = typename string_type::const_iterator;
      using regex_it    = regex_iterator<str_it>;

      bool first_only (
        static_cast<int> (flags & regex_constants::format_first_only) != 0);

      bool no_copy (
        static_cast<int> (flags & regex_constants::format_no_copy) != 0);

      // Note that by default the std::regex_search(), std::regex_replace(),
      // and std::regex_iterator() functions match the empty substrings in
      // non-empty strings for all the major implementations. For example:
      //
      // - regex_search("bb", "a*") call returns true.
      //
      // - regex_replace("bb", "a*", "x") call returns "xbxbx".
      //
      // - regex_replace("a", ".*", "x") call returns "xx".
      //
      // - Iterating using the regex_iterator("a", ".*") object ends up with
      //   the two matches: "a" and "".
      //
      // Since such a behavior feels counter-intuitive, we suppress it using
      // the match_not_null flag, except for the empty string.
      //
      if (!s.empty ())
        flags |= regex_constants::match_not_null;

      // Beginning of the last unmatched substring.
      //
      str_it ub (s.begin ());

      regex_it b (s.begin (), s.end (), re, flags);
      regex_it e;
      bool match (b != e);

      for (regex_it i (b); i != e; ++i)
      {
        const match_results<str_it>& m (*i);

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
    std::pair<basic_regex<C>, typename regex_char_traits<C>::string_type>
    regex_replace_parse (const C* s, size_t n,
                         regex_constants::syntax_option_type f)
    {
      using string_type = typename regex_char_traits<C>::string_type;

      std::size_t e;
      std::pair<string_type, string_type> r (regex_replace_parse (s, n, e));

      if (e != n)
        throw std::invalid_argument ("junk after trailing delimiter");

      return std::make_pair (basic_regex<C> (r.first, f), std::move (r.second));
    }

    template <typename C>
    std::pair<typename regex_char_traits<C>::string_type,
              typename regex_char_traits<C>::string_type>
    regex_replace_parse (const C* s, size_t n, size_t& e)
    {
      using std::invalid_argument;

      using string_type = typename regex_char_traits<C>::string_type;

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

      // Position to the format first character and find the trailing
      // delimiter.
      //
      n -= p - s + 1;
      s  = p + 1;

      p = string_type::traits_type::find (s, n, delim);

      if (p == nullptr)
        throw invalid_argument ("no delimiter after replacement");

      e = p - b + 1;
      return std::make_pair (std::move (re), string_type (s, p - s));
    }
  }
}
