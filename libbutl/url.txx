// file      : libbutl/url.txx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

LIBBUTL_MODEXPORT namespace butl //@@ MOD Clang needs this for some reason.
{
  // Convenience functions.
  //
  template <typename C>
  inline bool
  url_path_char (C c)
  {
    using url = basic_url<std::basic_string<C>>;

    return c == '/' || c == ':' || url::unreserved (c) ||
           c == '@' || url::sub_delim (c);
  }

  // basic_url_host
  //
  template <typename S>
  basic_url_host<S>::
  basic_url_host (string_type v)
  {
    using std::invalid_argument;

    using url       = basic_url<string_type>;
    using char_type = typename string_type::value_type;

    kind = v[0] == '[' ? kind_type::ipv6 : kind_type::name;

    if (kind == url_host_kind::ipv6)
    {
      if (v.back () != ']')
        throw invalid_argument ("invalid IPv6 address");

      value.assign (v, 1, v.size () - 2);
    }
    else
    {
      // Detect the IPv4 address host type.
      //
      {
        size_t n (0);
        string_type oct;

        auto ipv4_oct = [&oct, &n] () -> bool
        {
          if (n == 4 || oct.empty () || oct.size () > 3 || stoul (oct) > 255)
            return false;

          ++n;
          oct.clear ();
          return true;
        };

        auto i (v.cbegin ());
        auto e (v.cend ());

        for (; i != e; ++i)
        {
          char_type c (*i);

          if (digit (c))
            oct += c;
          else if (c != '.' || !ipv4_oct ())
            break;
        }

        if (i == e && ipv4_oct () && n == 4)
          kind = url_host_kind::ipv4;
      }

      // Verify and decode the host name.
      //
      bool dec (false);
      if (kind == url_host_kind::name)
      {
        for (auto c: v)
        {
          if (!(url::unreserved (c) || url::sub_delim (c) || c == '%'))
            throw invalid_argument ("invalid host name");

          if (c == '%')
            dec = true;
        }
      }

      value = dec ? url::decode (v) : move (v);
    }
  }

  template <typename S>
  S basic_url_host<S>::
  string () const
  {
    using url       = basic_url<string_type>;
    using char_type = typename string_type::value_type;

    if (empty ())
      return string_type ();

    switch (kind)
    {
    case url_host_kind::ipv4: return value;
    case url_host_kind::ipv6:
      {
        string_type r;
        r += '[';
        r += value;
        r += ']';
        return r;
      }
    case url_host_kind::name:
      {
        // We don't encode all characters that are disallowed for the host
        // part as RFC3986 requests:
        //
        // URI producing applications must not use percent-encoding in host
        // unless it is used to represent a UTF-8 character sequence.
        //
        // The callback requests to encode characters outside the ASCII
        // character set.
        //
        return url::encode (value,
                            [] (char_type& c)
                            {
                              // Convert to the unsigned numeric type, that is
                              // long enough to hold any character type.
                              //
                              return static_cast<unsigned long> (c) >= 0x80;
                            });
      }
    }

    assert (false); // Can't be here.
    return string_type ();
  }

  // basic_url_authority
  //
  template <typename S>
  S
  port_string (std::uint16_t p);

  template <>
  inline std::string
  port_string (std::uint16_t p)
  {
    return std::to_string (p);
  }

  template <>
  inline std::wstring
  port_string (std::uint16_t p)
  {
    return std::to_wstring (p);
  }

  template <typename S>
  S basic_url_authority<S>::
  string () const
  {
    if (empty ())
      return string_type ();

    string_type r;
    if (!user.empty ())
    {
      r += user;
      r += '@';
    }

    r += host.string ();

    if (port != 0)
    {
      r += ':';
      r += port_string<string_type> (port);
    }

    return r;
  }

  // basic_url
  //
  template <typename S, typename T>
  basic_url<S, T>::
  basic_url (const string_type& u)
  {
    using namespace std;

    using iterator  = typename string_type::const_iterator;

    // Create an empty URL object for the empty argument. Note that the scheme
    // is default-constructed, and so may stay undefined in this case.
    //
    if (u.empty ())
      return;

    try
    {
      // At the end of a component parsing 'i' points to the next component
      // start, and 'b' stays unchanged.
      //
      iterator b (u.cbegin ());
      iterator i (b);
      iterator e (u.cend ());

      // Extract scheme.
      //
      for(char_type c; i != e && (c = *i) != ':'; ++i)
      {
        if (!(i == b
              ? alpha (c)
              : (alnum (c) || c == '+' || c == '-' || c == '.')))
          throw invalid_argument ("invalid scheme");
      }

      if (i == b || i == e || i == b + 1) // Forbids one letter length schemes.
        throw invalid_argument ("no scheme");

      string_type sc (b, i++); // Skip ':'.

      // Parse authority.
      //
      if (i != e && i + 1 != e && *i == '/' && *(i + 1) == '/')
      {
        i += 2; // Skip '//'.

        // Find the authority end.
        //
        size_t p (u.find_first_of (string_type ({'/', '?', '#'}), i - b));
        iterator ae (p != string_type::npos ? b + p : e);

        string_type auth (i, ae);
        i = ae;

        // Extract user information.
        //
        string_type user;
        p = auth.find ('@');
        if (p != string_type::npos)
        {
          // Don't URL-decode the user information (scheme-specific).
          //
          user = string_type (auth, 0, p);
          auth = string_type (auth, p + 1);
        }

        // Extract host.
        //
        string_type host;
        p = auth.find_last_of({']', ':'}); // Note: ':' can belong to IPv6.

        if (p != string_type::npos && auth[p] == ']') // There is no port.
          p = string_type::npos;

        if (p != string_type::npos)
        {
          host = string_type (auth, 0, p);
          auth = string_type (auth, p + 1);
        }
        else
        {
          host = move (auth);
          auth = string_type ();
        }

        // Extract port.
        //
        uint16_t port (0);
        if (!auth.empty ())
        {
          auto bad_port = [] () {throw invalid_argument ("invalid port");};

          for (auto c: auth)
          {
            if (!digit (c))
              bad_port ();
          }

          unsigned long long n (stoull (auth));
          if (n == 0 || n > UINT16_MAX)
            bad_port ();

          port = static_cast<uint16_t> (n);
        }

        // User information and port are only meaningful if the host part is
        // present.
        //
        if (host.empty () && (!user.empty () || port != 0))
          throw invalid_argument ("no host");

        authority = {move (user), host_type (move (host)), port};
      }

      // Extract path.
      //
      if (i != e && *i == '/')
      {
        ++i; // Skip '/'.

        // Verify and URL-decode the path.
        //
        iterator j (i);
        for(char_type c; j != e && (c = *j) != '?' && c != '#'; ++j)
        {
          if (!(url_path_char (c) || c == '%'))
            throw invalid_argument ("invalid path");
        }

        // Note that encoding for non-ASCII path is not specified (in contrast
        // to the host name), and presumably is local to the referenced
        // authority.
        //
        string_type s;
        decode (i, j, back_inserter (s));
        path = traits::translate_path (move (s));
        i = j;
      }

      // Extract query.
      //
      if (i != e && *i == '?')
      {
        ++i; // Skip '?'.

        // Find the query component end.
        //
        size_t p (u.find ('#', i - b));
        iterator qe (p != string_type::npos ? b + p : e);

        // Don't URL-decode the query (scheme-specific).
        //
        query = string_type (i, qe);
        i = qe;
      }

      // We don't suppose to end up with an empty URL.
      //
      if (empty ())
        throw invalid_argument ("no authority, path or query");

      // Parse fragment.
      //
      if (i != e)
      {
        ++i; // Skip '#'.

        // Don't URL-decode the fragment (media type-specific).
        //
        fragment = string_type (i, e);
        i = e;
      }

      assert (i == e);

      // Translate the scheme string representation to its type.
      //
      scheme = traits::translate_scheme (u,
                                         move (sc),
                                         authority,
                                         path,
                                         query,
                                         fragment);
    }
    // If we fail to parse the URL, then delegate this job to
    // traits::translate_scheme(). If it also fails, leaving the components
    // absent, then we re-throw.
    //
    catch (const invalid_argument&)
    {
      authority = nullopt;
      path      = nullopt;
      query     = nullopt;
      fragment  = nullopt;

      scheme = traits::translate_scheme (u,
                                         string_type () /* scheme */,
                                         authority,
                                         path,
                                         query,
                                         fragment);

      if (!authority && !path && !query && !fragment)
        throw;
    }
  }

  template <typename S, typename T>
  typename basic_url<S, T>::string_type basic_url<S, T>::
  string () const
  {
    if (empty ())
      return string_type ();

    string_type u;
    string_type r (traits::translate_scheme (u,
                                             scheme,
                                             authority,
                                             path,
                                             query,
                                             fragment));

    // Return the custom URL pbject representation if provided.
    //
    if (!u.empty ())
      return u;

    r += ':';

    if (authority)
    {
      // We can't append '//' string literal, so appending characters.
      //
      r += '/';
      r += '/';

      r += authority->string ();
    }

    if (path)
    {
      r += '/';
      r += encode (traits::translate_path (*path),
                   [] (char_type& c) {return !url_path_char (c);});
    }

    if (query)
    {
      r += '?';
      r += *query;
    }

    if (fragment)
    {
      r += '#';
      r += *fragment;
    }

    return r;
  }

  template <typename S, typename T>
  template <typename I, typename O, typename F>
  void basic_url<S, T>::
  encode (I b, I e, O o, F&& f)
  {
    const char_type digits[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8',
                                '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    for (; b != e; ++b)
    {
      char_type c (*b);

      if (c == '%' || f (c))
      {
        assert (c == *b); // Must not be custom-encoded.

        *o++ = '%';
        *o++ = digits[(c >> 4) & 0xF];
        *o++ = digits[c & 0xF];
      }
      else
      {
        assert (c != '%'); // Otherwise decoding will be ambiguous.
        *o++ = c;
      }
    }
  }

  template <typename S, typename T>
  template <typename I, typename O, typename F>
  void basic_url<S, T>::
  decode (I b, I e, O o, F&& f)
  {
    using namespace std;

    for (; b != e; ++b)
    {
      char_type c (*b);

      // URL-decode the character.
      //
      if (c == '%')
      {
        // Note that we can't use (potentially more efficient) strtoul() here
        // as it doesn't have an overload for the wide character string.
        // However, the code below shouldn't be inefficient, given that the
        // string is short, and so is (probably) stack-allocated.
        //
        // Note that stoul() throws if no conversion could be performed, so we
        // explicitly check for xdigits.
        //
        if (++b != e && xdigit (*b) && b + 1 != e && xdigit (*(b + 1)))
          c = static_cast<char_type> (stoul (string_type (b, b + 2),
                                             nullptr,
                                             16));
        else
          throw invalid_argument ("invalid URL-encoding");

        ++b; // Position to the second xdigit.
      }
      else
        f (c);

      *o++ = c;
    }
  }
}
