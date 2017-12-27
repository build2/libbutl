// file      : tests/url/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules
#include <string>
#include <utility>   // move()
#include <iostream>
#include <iterator>  // back_inserter
#include <stdexcept> // invalid_argument
#endif

// Other includes.

#ifdef __cpp_modules
#ifdef __cpp_lib_modules
import std.core;
import std.io;
#endif
import butl.url;
import butl.utility; // operator<<(ostream, exception)
#else
#include <libbutl/url.mxx>
#include <libbutl/utility.mxx>
#endif

using namespace std;
using namespace butl;

enum class scheme
{
  http,
  https,
  file
};

namespace butl
{
  template <>
  struct url_traits<scheme>
  {
    using string_type = wstring;
    using path_type   = wstring;

    using scheme_type    = scheme;
    using authority_type = basic_url_authority<string_type>;

    static scheme_type
    translate_scheme (const string_type&         url,
                      string_type&&              scheme,
                      optional<authority_type>&  /*authority*/,
                      optional<path_type>&       path,
                      optional<string_type>&     /*query*/,
                      optional<string_type>&     /*fragment*/)
    {
      // Note that we must compare case-insensitive in the real program.
      //
      if (scheme == L"http")
        return scheme_type::http;
      else if (scheme == L"https")
        return scheme_type::https;
      else if (scheme == L"file")
        return scheme_type::file;
      else if (scheme.empty ())
      {
        // If the URL looks like an absolute filesystem path, then translate it
        // to the file URL. If it is not, then leave all the components absent
        // to fail with a proper exception description.
        //
        wchar_t c;
        if ((c = url[0]) == '/' ||
            (url.size () > 2 && alpha (c) && url[1] == ':' && url[2] == '/'))
          path = url;

        return scheme_type::file;
      }
      else
        throw invalid_argument ("unknown scheme");
    }

    // Translate scheme type back to its string representation.
    //
    static string_type
    translate_scheme (string_type&,                    /*url*/
                      const scheme_type& scheme,
                      const optional<authority_type>&  /*authority*/,
                      const optional<path_type>&       /*path*/,
                      const optional<string_type>&     /*query*/,
                      const optional<string_type>&     /*fragment*/)
    {
      switch (scheme)
      {
      case scheme_type::http:  return L"http";
      case scheme_type::https: return L"https";
      case scheme_type::file:  return L"file";
      }

      assert (false); // Can't be here.
      return L"";
    }

    static path_type
    translate_path (string_type&& path)
    {
      return path_type (move (path));
    }

    static string_type
    translate_path (const path_type& path) {return string_type (path);}
  };
}

// Usages:
//
// argv[0]
// argv[0] [-c|-s|-w] <url>
//
// Perform some basic tests if no URL is provided. Otherwise round-trip the URL
// to STDOUT. URL must contain only ASCII characters. Exit with zero code on
// success. Exit with code one on parsing failure, printing error description
// to STDERR.
//
// -c
//    Print the URL components one per line. Print the special '[null]' string
//    for an absent components. This is the default option if URL is provided.
//
// -s
//    Print stringified url object representation.
//
// -w
//    Same as above, but use the custom wstring-based url_traits
//    implementation for the basic_url template.
//
int
main (int argc, const char* argv[])
try
{
  using butl::nullopt; // Disambiguate with std::nullopt.

  using wurl           = basic_url<scheme>;
  using wurl_authority = wurl::authority_type;
  using wurl_host      = wurl::host_type;

  enum class print_mode
  {
    str,
    wstr,
    comp
  } mode (print_mode::comp);

  int i (1);
  for (; i != argc; ++i)
  {
    string o (argv[i]);
    if (o == "-s")
      mode = print_mode::str;
    else if (o == "-w")
      mode = print_mode::wstr;
    else if (o == "-c")
      mode = print_mode::comp;
    else
      break; // End of options.
  }

  if (i == argc)
  {
    // Test ctors and operators.
    //
    {
      wurl u0 ((wstring ()));
      assert (u0.empty ());
      assert (u0 == wurl ());

      wurl u1 (scheme::http,
               wurl_authority {wstring (), wurl_host (L"[123]"), 0},
               wstring (L"login"),
               wstring (L"q="),
               wstring (L"f"));

      assert (!u1.empty ());
      assert (u1 != u0);

      wurl u2 (scheme::http,
               wurl_host (L"123", url_host_kind::ipv6),
               wstring (L"login"),
               wstring (L"q="),
               wstring (L"f"));

      assert (u2 == u1);

      wurl u3 (scheme::http,
               wurl_host (L"123", url_host_kind::ipv6),
               0,
               wstring (L"login"),
               wstring (L"q="),
               wstring (L"f"));

      assert (u3 == u2);

      wurl u4 (scheme::http,
               L"[123]",
               wstring (L"login"),
               wstring (L"q="),
               wstring (L"f"));

      assert (u4 == u3);

      wurl u5 (scheme::http,
               L"[123]",
               0,
               wstring (L"login"),
               wstring (L"q="),
               wstring (L"f"));

      assert (u5 == u4);
    }

    // Test encode and decode.
    //
    {
      const char* s ("ABC +");
      string es (url::encode (s));

      assert (es == "ABC%20%2B");
      string ds (url::decode (es));

      assert (ds == s);
    }

    {
      const char* s ("ABC +");

      string es (url::encode (s,
                              [] (char& c) -> bool
                              {
                                if (c == ' ')
                                {
                                  c = '+';
                                  return false;
                                }
                                return !url::unreserved (c);
                              }));

      assert (es == "ABC+%2B");

      string ds (url::decode (es.c_str (),
                              [] (char& c)
                              {
                                if (c == '+')
                                  c = ' ';
                              }));
      assert (ds == s);
    }
    {
      const wchar_t s[] = L"ABC ";

      wstring es;
      wurl::encode (s, s + 4,
                    back_inserter (es),
                    [] (wchar_t& c) -> bool
                    {
                      if (!alnum (c))
                        return true;

                      ++c;
                      return false;
                    });
      assert (es == L"BCD%20");

      wstring ds (wurl::decode (es,
                                [] (wchar_t& c)
                                {
                                  if (alnum (c))
                                    --c;
                                }));
      assert (ds == s);
    }
  }
  else // Round-trip the URL.
  {
    assert (i + 1 == argc);

    const char* ua (argv[i]);

    switch (mode)
    {
    case print_mode::str:
      {
        cout << url (ua) << endl;
        break;
      }
    case print_mode::wstr:
      {
        // Convert ASCII string to wstring.
        //
        wstring s (ua, ua + strlen (ua));

        wcout << wurl (s) << endl;
        break;
      }
    case print_mode::comp:
      {
        // Convert ASCII string to wstring.
        //
        wstring s (ua, ua + strlen (ua));
        wurl u (s);

        if (!u.empty ())
        {
          wstring s;
          wcout << wurl::traits::translate_scheme (s,
                                                   u.scheme,
                                                   nullopt,
                                                   nullopt,
                                                   nullopt,
                                                   nullopt) << endl;
        }
        else
          wcout << L"[null]" << endl;

        if (u.authority)
        {
          const wchar_t* kinds[] = {L"ipv4", L"ipv6", L"name"};
          const wurl_authority& a (*u.authority);

          wcout << a.user << L'@' << a.host.value << L':' << a.port
                << " " << kinds[static_cast<size_t> (a.host.kind)] << endl;
        }
        else
          wcout << L"[null]" << endl;

        wcout << (u.path     ? *u.path     : L"[null]") << endl
              << (u.query    ? *u.query    : L"[null]") << endl
              << (u.fragment ? *u.fragment : L"[null]") << endl;
        break;
      }
    }
  }

  return 0;
}
catch (const invalid_argument& e)
{
  cerr << e << endl;
  return 1;
}
