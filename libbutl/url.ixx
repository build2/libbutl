// file      : libbutl/url.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

LIBBUTL_MODEXPORT namespace butl //@@ MOD Clang needs this for some reason.
{
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
}
