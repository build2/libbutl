// file      : libbutl/url.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  // url_traits
  //
  template <typename H, typename S, typename P>
  inline typename url_traits<H, S, P>::path_type url_traits<H, S, P>::
  translate_path (string_type&& path)
  {
    return path_type (basic_url<string_type>::decode (path));
  }

  template <typename H, typename S, typename P>
  inline typename url_traits<H, S, P>::string_type url_traits<H, S, P>::
  translate_path (const path_type& path)
  {
    using url = basic_url<string_type>;

    return url::encode (
      string_type (path),
      [] (typename url::char_type& c) {return !url::path_char (c);});
  }

  // basic_url
  //
  template <typename S, typename T>
  inline basic_url<S, T>::
  basic_url (scheme_type s,
             optional<authority_type> a,
             optional<path_type> p,
             optional<string_type> q,
             optional<string_type> f)
      : scheme (std::move (s)),
        authority (std::move (a)),
        path (std::move (p)),
        query (std::move (q)),
        fragment (std::move (f))
  {
  }

  template <typename S, typename T>
  inline basic_url<S, T>::
  basic_url (scheme_type s,
             host_type h,
             optional<path_type> p,
             optional<string_type> q,
             optional<string_type> f)
      : basic_url (std::move (s),
                   authority_type {string_type (), std::move (h), 0},
                   std::move (p),
                   std::move (q),
                   std::move (f))
  {
  }

  template <typename S, typename T>
  inline basic_url<S, T>::
  basic_url (scheme_type s,
             host_type h,
             std::uint16_t o,
             optional<path_type> p,
             optional<string_type> q,
             optional<string_type> f)
      : basic_url (std::move (s),
                   authority_type {string_type (), std::move (h), o},
                   std::move (p),
                   std::move (q),
                   std::move (f))
  {
  }

  template <typename S, typename T>
  inline basic_url<S, T>::
  basic_url (scheme_type s,
             string_type h,
             optional<path_type> p,
             optional<string_type> q,
             optional<string_type> f)
      : basic_url (std::move (s),
                   host_type (std::move (h)),
                   std::move (p),
                   std::move (q),
                   std::move (f))
  {
  }

  template <typename S, typename T>
  inline basic_url<S, T>::
  basic_url (scheme_type s,
             string_type h,
             std::uint16_t o,
             optional<path_type> p,
             optional<string_type> q,
             optional<string_type> f)
      : basic_url (std::move (s),
                   host_type (std::move (h)),
                   o,
                   std::move (p),
                   std::move (q),
                   std::move (f))
  {
  }

  template <typename S, typename T>
  inline basic_url<S, T>::
  basic_url (scheme_type s,
             optional<path_type> p,
             optional<string_type> q,
             optional<string_type> f)
      : scheme (std::move (s)),
        path (std::move (p)),
        query (std::move (q)),
        fragment (std::move (f)),
        rootless (true)
  {
  }

  template <typename S, typename T>
  inline void basic_url<S, T>::
  normalize ()
  {
    if (authority)
      authority->host.normalize ();
  }
}
