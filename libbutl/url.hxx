// file      : libbutl/url.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <cassert>
#include <cstddef>  // size_t
#include <cstdint>  // uint*_t
#include <utility>  // move()
#include <ostream>
#include <iterator> // back_inserter

#include <libbutl/path.hxx>
#include <libbutl/utility.hxx>
#include <libbutl/optional.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // RFC3986 Uniform Resource Locator (URL).
  //
  // <url> = <scheme>:[//[<authority>]][/<path>][?<query>][#<fragment>] |
  //         <scheme>:[<path>][?<query>][#<fragment>]
  //
  // <authority> = [<user>@]<host>[:<port>]
  //
  // Some examples of equivalent URLs to meditate upon:
  //
  // file://localhost/tmp     (localhost authority)
  // file:///tmp              (empty     authority)
  // file:/tmp                (absent    authority)
  //
  // file://localhost/c:/tmp
  // file:///c:/tmp
  // file:/c:/tmp
  //
  // We think of the slash between <authority> and <path> as a separator but
  // with the path always interpreted as starting from the "root" of the
  // authority. Thus:
  //
  // file://localhost/tmp     ->  'file'://'localhost'/'tmp'    ->  /tmp
  // file://localhost/c:/tmp  ->  'file'://'localhost'/'c:/tmp' ->  c:/tmp
  //
  // This means that the <path> component is represented as a relative path
  // and, in the general case, we cannot use our path type for its storage
  // since it assumes the path is for the host platform. In other words, the
  // interpretation of the path has to take into account the platform of the
  // authority host. Note, however, that a custom url_traits implementation
  // can choose to use the path type if local paths are to be interpreted as
  // relative to the host.
  //
  // For authority-less schemes the <path> component is also represented as a
  // relative path. Some examples of such URLs (let's call them rootless
  // rather than authority-less not to confuse with a case where authority is
  // empty/implied):
  //
  // pkcs11:token=sign;object=SIGN%20key
  // pkcs11:id=%02%38%01?pin-value=12345
  // pkcs11:
  //
  // Note that a scheme can theoretically allow both rootless and "rootfull"
  // representations.
  //
  // Note also that we currently forbid one character schemes to support
  // scheme-less (Windows) paths which can be done by
  // url_traits::translate_scheme() (see below). (A Windows path that uses
  // forward slashes would be parsed as a valid authority-less URL).

  // URL host component can be an IPv4 address (if matches its dotted-decimal
  // notation), an IPv6 address (if enclosed in [square brackets]) or
  // otherwise a name.
  //
  // Note that non-ASCII host names are allowed in URLs. They must be
  // UTF8-encoded and URL-encoded afterwards. Curently we store the parsed
  // host name UTF8-encoded without regards to the template argument string
  // type. Later we may add support for more appropriate encodings for
  // multi-byte character types.
  //
  enum class url_host_kind {ipv4, ipv6, name};

  template <typename S>
  struct basic_url_host
  {
    using string_type = S;
    using kind_type   = url_host_kind;

    string_type value;
    kind_type   kind;

    // Can be treated as const string_type&.
    //
    operator const string_type& () const noexcept {return value;}

    // Create an empty host.
    //
    basic_url_host (): kind (kind_type::name) {}

    // Create the host object from its string representation as it appears in
    // a URL, throwing std::invalid_argument if invalid. Remove the enclosing
    // square brackets for IPv6 addresses, and URL-decode host names.
    //
    // Note that the 'x:x:x:x:x:x:d.d.d.d' IPv6 address mixed notation is not
    // supported.
    //
    explicit
    basic_url_host (string_type);

    basic_url_host (string_type v, kind_type k)
        : value (std::move (v)), kind (k) {}

    bool
    empty () const
    {
      assert (kind == kind_type::name || !value.empty ());
      return value.empty ();
    }

    // Return string representation of the host as it would appear in a URL.
    //
    string_type
    string () const;

    // Normalize the host value in accordance with its type:
    //
    // Name - convert to the lower case. Note: only ASCII names are currently
    //        supported.
    //
    // IPv4 - strip the leading zeros in its octets.
    //
    // IPv6 - strip the leading zeros in its groups (hextets), squash the
    //        longest zero-only hextet sequence, and convert to the lower case
    //        (as per RFC5952).
    //
    // Assume that the host value is valid.
    //
    void
    normalize ();
  };

  template <typename S>
  struct basic_url_authority
  {
    using string_type = S;
    using host_type   = basic_url_host<string_type>;

    string_type   user;  // Empty if not specified.
    host_type     host;
    std::uint16_t port;  // Zero if not specified.

    bool
    empty () const
    {
      assert (!host.empty () || (user.empty () && port == 0));
      return host.empty ();
    }

    // Return a string representation of the URL authority. String
    // representation of an empty instance is the empty string.
    //
    string_type
    string () const;
  };

  template <typename H, typename S = H, typename P = S>
  struct url_traits
  {
    using scheme_type = H;
    using string_type = S;
    using path_type   = P;

    using authority_type = basic_url_authority<string_type>;

    // Translate the scheme string representation to its type. May throw
    // std::invalid_argument. May change the URL components. Should not return
    // nullopt if called with a non-empty scheme.
    //
    // This function is called with an empty scheme if the URL has no scheme,
    // the scheme is invalid, or it could not be parsed into components
    // according to the URL syntax. In this case all the passed components
    // reference empty/absent/false values. If nullopt is returned, the URL is
    // considered invalid and the std::invalid_argument exception with an
    // appropriate description is thrown by the URL object constructor. This
    // can be used to support scheme-less URLs, local paths, etc.
    //
    static optional<scheme_type>
    translate_scheme (const string_type&         /*url*/,
                      string_type&&              scheme,
                      optional<authority_type>&  /*authority*/,
                      optional<path_type>&       /*path*/,
                      optional<string_type>&     /*query*/,
                      optional<string_type>&     /*fragment*/,
                      bool&                      /*rootless*/)
    {
      return !scheme.empty ()
        ? optional<scheme_type> (std::move (scheme))
        : nullopt; // Leave the URL object constructor to throw.
    }

    // Translate scheme type back to its string representation.
    //
    // Similar to the above the function is called with an empty string
    // representation. If on return this value is no longer empty, then it is
    // assume the URL has been translated in a custom manner (in which case
    // the returned scheme value is ignored).
    //
    static string_type
    translate_scheme (string_type&,                    /*url*/
                      const scheme_type&               scheme,
                      const optional<authority_type>&  /*authority*/,
                      const optional<path_type>&       /*path*/,
                      const optional<string_type>&     /*query*/,
                      const optional<string_type>&     /*fragment*/,
                      bool                             /*rootless*/)
    {
      return string_type (scheme);
    }

    // Translate the URL-encoded path string representation to its type.
    //
    // Note that encoding for non-ASCII paths is not specified (in contrast
    // to the host name), and presumably is local to the referenced authority.
    // Furthermore, for some schemes, the path component can contain encoded
    // binary data, for example for pkcs11.
    //
    static path_type
    translate_path (string_type&&);

    // Translate path type back to its URL-encoded string representation.
    //
    static string_type
    translate_path (const path_type&);

    // Check whether a string looks like a non-rootless URL by searching for
    // the first ':' (unless its position is specified with the second
    // argument) and then making sure it's both followed by '/' (e.g., http://
    // or file:/) and preceded by a valid scheme at least 2 characters long
    // (so we don't confuse it with an absolute Windows path, e.g., c:/).
    //
    // Return the start of the URL substring or string_type::npos.
    //
    static std::size_t
    find (const string_type&, std::size_t pos = string_type::npos);
  };

  template <typename H, // scheme
            typename T = url_traits<H>>
  class basic_url
  {
  public:
    using traits_type = T;

    using string_type = typename traits_type::string_type;
    using char_type   = typename string_type::value_type;
    using path_type   = typename traits_type::path_type;

    using scheme_type    = typename traits_type::scheme_type;
    using authority_type = typename traits_type::authority_type;
    using host_type      = typename authority_type::host_type;

    scheme_type              scheme;
    optional<authority_type> authority;
    optional<path_type>      path;
    optional<string_type>    query;
    optional<string_type>    fragment;
    bool                     rootless = false;

    // Create an empty URL object.
    //
    basic_url (): scheme (), empty_ (true) {}

    // Create the URL object from its string representation. Verify that the
    // string is compliant to the generic URL syntax. URL-decode and validate
    // components with common for all schemes syntax (scheme, host, port).
    // Throw std::invalid_argument if the passed string is not a valid URL
    // representation.
    //
    // Validation and URL-decoding of the scheme-specific components can be
    // provided by a custom url_traits::translate_scheme() implementation.
    //
    explicit
    basic_url (const string_type&);

    // Create the URL object from individual components. Performs no
    // components URL-decoding or verification.
    //
    basic_url (scheme_type,
               optional<authority_type>,
               optional<path_type> path,
               optional<string_type> query = nullopt,
               optional<string_type> fragment = nullopt);

    basic_url (scheme_type,
               host_type host,
               optional<path_type> path,
               optional<string_type> query = nullopt,
               optional<string_type> fragment = nullopt);

    basic_url (scheme_type,
               host_type host,
               std::uint16_t port,
               optional<path_type> path,
               optional<string_type> query = nullopt,
               optional<string_type> fragment = nullopt);

    basic_url (scheme_type,
               string_type host,
               optional<path_type> path,
               optional<string_type> query = nullopt,
               optional<string_type> fragment = nullopt);

    basic_url (scheme_type,
               string_type host,
               std::uint16_t port,
               optional<path_type> path,
               optional<string_type> query = nullopt,
               optional<string_type> fragment = nullopt);

    // Create a rootless URL.
    //
    basic_url (scheme_type,
               optional<path_type> path,
               optional<string_type> query = nullopt,
               optional<string_type> fragment = nullopt);

    bool
    empty () const noexcept {return empty_;}

    // Return a string representation of the URL. Note that while this is not
    // necessarily syntactically the same string as what was used to
    // initialize this instance, it should be semantically equivalent. String
    // representation of an empty instance is the empty string.
    //
    string_type
    string () const;

    // Normalize the URL host, if present.
    //
    void
    normalize ();

    // The following predicates can be used to classify URL characters while
    // parsing, validating or encoding scheme-specific components. For the
    // semantics of character classes see RFC3986.
    //
    static bool
    gen_delim (char_type c)
    {
      return c == ':' || c == '/' || c == '?' || c == '#' || c == '[' ||
             c == ']' || c == '@';
    }

    static bool
    sub_delim (char_type c)
    {
      return c == '!' || c == '$' || c == '&' || c == '=' || c == '(' ||
             c == ')' || c == '*' || c == '+' || c == ',' || c == ';' ||
             c == '\'';
    }

    static bool
    reserved (char_type c) {return sub_delim (c) || gen_delim (c);}

    static bool
    unreserved (char_type c)
    {
      return alnum (c) || c == '-' ||  c == '.' || c =='_' || c == '~';
    }

    static bool
    path_char (char_type c)
    {
      return c == '/' || c == ':' || unreserved (c) || c == '@' ||
             sub_delim (c);
    }

    // URL-encode a character sequence.
    //
    // Note that the set of characters that should be encoded may differ for
    // different URL components. The optional callback function must return
    // true for characters that should be percent-encoded. The function may
    // encode the passed character in it's own way with another character (but
    // never with '%'), and return false. By default all characters other than
    // unreserved are percent-encoded.
    //
    // Also note that the characters are interpreted as bytes. In other words,
    // each character may result in a single encoding triplet.
    //
    template <typename I, typename O, typename F>
    static void
    encode (I begin, I end, O output, F&& efunc);

    template <typename I, typename O>
    static void
    encode (I b, I e, O o)
    {
      encode (b, e, o, [] (char_type& c) {return !unreserved (c);});
    }

    template <typename F>
    static string_type
    encode (const string_type& s, F&& f)
    {
      string_type r;
      encode (s.begin (), s.end (), std::back_inserter (r), f);
      return r;
    }

    static string_type
    encode (const string_type& s)
    {
      return encode (s, [] (char_type& c) {return !unreserved (c);});
    }

    template <typename F>
    static string_type
    encode (const char_type* s, F&& f)
    {
      string_type r;
      encode (s, s + string_type::traits_type::length (s),
              std::back_inserter (r),
              f);
      return r;
    }

    static string_type
    encode (const char_type* s)
    {
      return encode (s, [] (char_type& c) {return !unreserved (c);});
    }

    // URL-decode a character sequence. Throw std::invalid_argument if an
    // invalid encoding sequence is encountered.
    //
    // If some characters in the sequence are encoded with another characters
    // (rather than percent-encoded), then one must provide the callback
    // function to decode them.
    //
    template <typename I, typename O, typename F>
    static void
    decode (I begin, I end, O output, F&& dfunc);

    template <typename I, typename O>
    static void
    decode (I b, I e, O o)
    {
      decode (b, e, o, [] (char_type&) {});
    }

    template <typename F>
    static string_type
    decode (const string_type& s, F&& f)
    {
      string_type r;
      decode (s.begin (), s.end (), std::back_inserter (r), f);
      return r;
    }

    static string_type
    decode (const string_type& s)
    {
      return decode (s, [] (char_type&) {});
    }

    template <typename F>
    static string_type
    decode (const char_type* s, F&& f)
    {
      string_type r;
      decode (s, s + string_type::traits_type::length (s),
              std::back_inserter (r),
              f);
      return r;
    }

    static string_type
    decode (const char_type* s)
    {
      return decode (s, [] (char_type&) {});
    }

  private:
    bool empty_ = false;
  };

  using url_authority = basic_url_authority<std::string>;
  using url           = basic_url          <std::string>;

  template <typename S>
  inline bool
  operator== (const basic_url_host<S>& x, const basic_url_host<S>& y) noexcept
  {
    return x.value == y.value && x.kind == y.kind;
  }

  template <typename S>
  inline bool
  operator!= (const basic_url_host<S>& x, const basic_url_host<S>& y) noexcept
  {
    return !(x == y);
  }

  template <typename S>
  inline bool
  operator== (const basic_url_authority<S>& x,
              const basic_url_authority<S>& y) noexcept
  {
    return x.user == y.user && x.host == y.host && x.port == y.port;
  }

  template <typename S>
  inline bool
  operator!= (const basic_url_authority<S>& x,
              const basic_url_authority<S>& y) noexcept
  {
    return !(x == y);
  }

  template <typename S, typename T>
  inline bool
  operator== (const basic_url<S, T>& x, const basic_url<S, T>& y) noexcept
  {
    if (x.empty () || y.empty ())
      return x.empty () == y.empty ();

    return x.scheme    == y.scheme    &&
           x.authority == y.authority &&
           x.path      == y.path      &&
           x.query     == y.query     &&
           x.fragment  == y.fragment  &&
           x.rootless  == y.rootless;
  }

  template <typename S, typename T>
  inline bool
  operator!= (const basic_url<S, T>& x, const basic_url<S, T>& y) noexcept
  {
    return !(x == y);
  }

  template <typename S, typename T>
  inline auto
  operator<< (std::basic_ostream<typename T::string_type::value_type>& o,
              const basic_url<S, T>& u) -> decltype (o)
  {
    return o << u.string ();
  }
}

#include <libbutl/url.ixx>
#include <libbutl/url.txx>
