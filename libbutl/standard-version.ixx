// file      : libbutl/standard-version.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline std::uint16_t standard_version::
  major () const noexcept
  {
    std::uint64_t e (version % 10);
    std::uint64_t v (version / 10);
    std::uint64_t ab (v % 1000);
    if (ab != 0 || e == 1)
      v += 1000 - ab;

    return static_cast<std::uint16_t> (v / 1000000000 % 1000);
  }

  inline std::uint16_t standard_version::
  minor () const noexcept
  {
    std::uint64_t e (version % 10);
    std::uint64_t v (version / 10);
    std::uint64_t ab (v % 1000);
    if (ab != 0 || e == 1)
      v += 1000 - ab;

    return static_cast<std::uint16_t> (v / 1000000 % 1000);
  }

  inline std::uint16_t standard_version::
  patch () const noexcept
  {
    std::uint64_t e (version % 10);
    std::uint64_t v (version / 10);
    std::uint64_t ab (v % 1000);
    if (ab != 0 || e == 1)
      v += 1000 - ab;

    return static_cast<std::uint16_t> (v / 1000 % 1000);
  }

  inline std::uint16_t standard_version::
  pre_release () const noexcept
  {
    std::uint64_t ab (version / 10 % 1000);
    if (ab > 500)
      ab -= 500;

    return static_cast<std::uint16_t> (ab);
  }

  inline bool standard_version::
  alpha () const noexcept
  {
    std::uint64_t abe (version % 10000);
    return abe > 0 && abe < 5000 && !stub ();
  }

  inline bool standard_version::
  beta () const noexcept
  {
    std::uint64_t abe (version % 10000);
    return abe > 5000 && !stub ();
  }

  inline bool standard_version::
  earliest () const noexcept
  {
    return version % 10000 == 1 && !snapshot () && !stub ();
  }

  inline standard_version::flags
  operator& (standard_version::flags x, standard_version::flags y)
  {
    return x &= y;
  }

  inline standard_version::flags
  operator| (standard_version::flags x, standard_version::flags y)
  {
    return x |= y;
  }

  inline standard_version::flags
  operator&= (standard_version::flags& x, standard_version::flags y)
  {
    return x = static_cast<standard_version::flags> (
      static_cast<std::uint16_t> (x) &
      static_cast<std::uint16_t> (y));
  }

  inline standard_version::flags
  operator|= (standard_version::flags& x, standard_version::flags y)
  {
    return x = static_cast<standard_version::flags> (
      static_cast<std::uint16_t> (x) |
      static_cast<std::uint16_t> (y));
  }
}