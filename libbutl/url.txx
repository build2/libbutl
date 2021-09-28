// file      : libbutl/url.txx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <stdexcept> // invalid_argument
#include <algorithm> // find(), find_if()

#include <libbutl/small-vector.hxx>

namespace butl
{
  // Convenience functions.
  //
  // basic_url_host
  //
  template <typename S>
  basic_url_host<S>::
  basic_url_host (string_type v)
  {
    using namespace std;

    using url       = basic_url<string_type>;
    using char_type = typename string_type::value_type;

    kind = v[0] == '[' ? kind_type::ipv6 : kind_type::name;

    // Note that an IPv6 address is represented as eight colon-separated
    // groups (hextets) of four or less hexadecimal digits. One or more
    // consecutive zero hextets can be represented by double-colon (squashed),
    // but only once, for example: 1::2:0:0:3.
    //
    if (kind == url_host_kind::ipv6)
    {
      auto bad_ip = [] () {throw invalid_argument ("invalid IPv6 address");};

      if (v.back () != ']')
        bad_ip ();

      // Validate the IPv6 address.
      //
      // If the address doesn't contain the double-colon, then we will verify
      // that it is composed of eight valid hextets. Otherwise, we will split
      // the address by the double-colon into two hextet sequences, validate
      // their hextets, and verify that their cumulative length is less than
      // eight.
      //
      using iter = typename string_type::const_iterator;

      // Validate a hextet sequence and return its length.
      //
      auto len = [&bad_ip] (iter b, iter e)
      {
        size_t r (0);

        if (b == e)
          return r;

        size_t n (0); // Current hextet length.

        // Fail if the current hextet is of an invalid length and increment
        // the sequence length counter otherwise.
        //
        auto validate = [&r, &n, &bad_ip] ()
        {
          if (n == 0 || n > 4)
            bad_ip ();

          ++r;
          n = 0;
        };

        for (iter i (b); i != e; ++i)
        {
          char_type c (*i);

          if (xdigit (c))
            ++n;
          else if (c == ':')
            validate ();
          else
            bad_ip ();
        }

        validate (); // Validate the trailing hextet.
        return r;
      };

      size_t p (v.find (string_type (2, ':'), 1));

      size_t n1 (p != string_type::npos
                 ? len (v.begin () + 1, v.begin () + p)
                 : len (v.begin () + 1, v.end () - 1));

      size_t n2 (p != string_type::npos
                 ? len (v.begin () + p + 2, v.end () - 1)
                 : 0);

      if (p != string_type::npos ? (n1 + n2 < 8) : (n1 == 8))
        value.assign (v, 1, v.size () - 2);
      else
        bad_ip ();
    }
    else // IPV4 or name.
    {
      // Detect the IPv4 address host type.
      //
      {
        size_t n (0);    // Number of octets.
        string_type oct; // Current octet.

        // Return true if the current octet is valid.
        //
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

  template <typename S>
  void basic_url_host<S>::
  normalize ()
  {
    using namespace std;

    using char_type = typename string_type::value_type;

    switch (kind)
    {
    case url_host_kind::name:
      {
        for (char_type& c: value)
          c = lcase (static_cast<char> (c));

        break;
      }
    case url_host_kind::ipv4:
      {
        // Strip the leading zeros in the octets.
        //
        string_type v; // Normalized address.
        size_t n (0);  // End of the last octet (including dot).

        for (char_type c: value)
        {
          if (c == '.')
          {
            // If no digits were added since the last octet was processed,
            // then the current octet is zero and so we add it.
            //
            if (n == v.size ())
              v += '0';

            v += '.';
            n = v.size ();
          }
          else if (c != '0' || n != v.size ()) // Not a leading zero?
            v += c;
        }

        // Handle the trailing zero octet.
        //
        if (n == v.size ())
          v += '0';

        value = move (v);
        break;
      }
    case url_host_kind::ipv6:
      {
        // The overall plan is to (1) normalize the address hextets by
        // converting them into lower case and stripping the leading zeros,
        // (2) expand the potentially present double-colon into the zero
        // hextet sequence, and (3) then squash the longest zero hextet
        // sequence into double-colon. For example:
        //
        // 0ABC::1:0:0:0:0 -> abc:0:0:1::

        // Parse the address into an array of normalized hextets.
        //
        // Note that if we meet the double-colon, we cannot expand it into the
        // zero hextet sequence right away, since its length is unknown at
        // this stage. Instead, we will save its index and expand it later.
        //
        small_vector<string_type, 8> v; // Normalized address.
        string_type hex;                // Current hextet.
        optional<size_t> dci;           // Double-colon index, if present.
        const string_type z (1, '0');   // Zero hextet.

        // True if any leading zeros are stripped for the current hextet.
        //
        bool stripped (false);

        auto add_hex = [&v, &hex, &stripped, &dci, &z] ()
        {
          if (!hex.empty ())
          {
            v.emplace_back (move (hex));
            hex.clear ();
          }
          else
          {
            if (!stripped)     // Double-colon?
              dci = v.size (); // Note: can be set twice to 0 (think of ::1).
            else
              v.push_back (z);
          }

          stripped = false;
        };

        for (char_type c: value)
        {
          if (c == ':')
            add_hex ();
          else if (c == '0' && hex.empty ()) // Leading zero?
            stripped = true;
          else
            hex += lcase (static_cast<char> (c));
        }

        // Handle the trailing hextet.
        //
        if (!hex.empty ())
          v.emplace_back (move (hex));
        else if (stripped)
          v.push_back (z);
        //
        // Else this is the trailing (already handled) double-colon.

        // Expand double-colon, if present.
        //
        if (dci)
        {
          if (v.size () < 8)
            v.insert (v.begin () + *dci, 8 - v.size (), z);
          else
            assert (false); // Too long address.
        }

        // Find the longest zero hextet sequence.
        //
        // Note that the first sequence of zeros is chosen between the two of
        // the same length.
        //
        // Also note that we don't squash the single zero.
        //
        using iter = typename small_vector<string_type, 8>::const_iterator;

        iter   e (v.end ());
        iter   mxi (e);      // Longest sequence start.
        iter   mxe;          // Longest sequence end.
        size_t mxn (1);      // Longest sequence length (must be > 1).

        for (iter i (v.begin ()); i != e; )
        {
          i = find (i, e, z);

          if (i != e)
          {
            iter ze (find_if (i + 1, e,
                              [&z] (const string_type& h) {return h != z;}));

            size_t n (ze - i);

            if (mxn < n)
            {
              mxn = n;
              mxi = i;
              mxe = ze;
            }

            i = ze;
          }
        }

        // Compose the IPv6 string, squashing the longest zero hextet
        // sequence, if present.
        //
        value.clear ();

        for (iter i (v.begin ()); i != e; )
        {
          if (i != mxi)
          {
            // Add ':', unless the hextet is the first or follows double-
            // colon.
            //
            if (!value.empty () && value.back () != ':')
              value += ':';

            value += *i++;
          }
          else
          {
            value.append (2, ':');
            i = mxe;
          }
        }

        break;
      }
    }
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

  // url_traits
  //
  template <typename H, typename S, typename P>
  std::size_t url_traits<H, S, P>::
  find (const string_type& s, std::size_t p)
  {
    if (p == string_type::npos)
      p = s.find (':');

    if (p == string_type::npos ||
        p < 2 ||
        p + 1 == s.size () || s[p + 1] != '/')
      return string_type::npos;

    // Scan backwards for as long as it is a valid scheme.
    //
    std::size_t i (p);

    for (; i != 0; --i)
    {
      auto c (s[i - 1]);
      if (!(alnum (c) || c == '+' || c == '-' || c == '.'))
        break;
    }

    if (i != p && !alpha (s[i])) // First must be alpha.
      ++i;

    if (p - i < 2)
      return string_type::npos;

    return i;
  }

  // basic_url
  //
  template <typename S, typename T>
  basic_url<S, T>::
  basic_url (const string_type& u)
  {
    using namespace std;

    using iterator = typename string_type::const_iterator;

    try
    {
      if (u.empty ())
        throw invalid_argument ("empty URL");

      // At the end of a component parsing 'i' points to the next component
      // start, and 'b' stays unchanged.
      //
      iterator b (u.cbegin ());
      iterator i (b);
      iterator e (u.cend ());

      // Extract scheme.
      //
      for (char_type c; i != e && (c = *i) != ':'; ++i)
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
      if (i != e && *i != '?' && *i != '#')
      {
        rootless = *i != '/';

        if (!rootless)
          ++i;

        // Verify and translate the path.
        //
        iterator j (i);
        for(char_type c; j != e && (c = *j) != '?' && c != '#'; ++j)
        {
          if (!(path_char (c) || c == '%'))
            throw invalid_argument ("invalid path");
        }

        path = traits_type::translate_path (string_type (i, j));
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
      optional<scheme_type> s (traits_type::translate_scheme (u,
                                                              move (sc),
                                                              authority,
                                                              path,
                                                              query,
                                                              fragment,
                                                              rootless));
      assert (s);
      scheme = *s;
    }
    // If we fail to parse the URL, then delegate this job to
    // traits::translate_scheme(). If it also fails, returning nullopt, then
    // we re-throw.
    //
    catch (const invalid_argument&)
    {
      authority = nullopt;
      path      = nullopt;
      query     = nullopt;
      fragment  = nullopt;
      rootless  = false;

      optional<scheme_type> s (
        traits_type::translate_scheme (u,
                                       string_type () /* scheme */,
                                       authority,
                                       path,
                                       query,
                                       fragment,
                                       rootless));
      if (!s)
        throw;

      scheme = *s;
    }
  }

  template <typename S, typename T>
  typename basic_url<S, T>::string_type basic_url<S, T>::
  string () const
  {
    if (empty ())
      return string_type ();

    string_type u;
    string_type r (traits_type::translate_scheme (u,
                                                  scheme,
                                                  authority,
                                                  path,
                                                  query,
                                                  fragment,
                                                  rootless));

    // Return the custom URL pbject representation if provided.
    //
    if (!u.empty ())
      return u;

    if (!r.empty ())
      r += ':';

    if (authority)
    {
      // We can't append '//' string literal, so appending characters.
      //
      if (!r.empty ())
      {
        r += '/';
        r += '/';
      }

      r += authority->string ();
    }

    if (path)
    {
      if (!rootless)
        r += '/';

      r += traits_type::translate_path (*path);
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
