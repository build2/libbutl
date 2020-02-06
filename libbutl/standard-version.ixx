// file      : libbutl/standard-version.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline std::uint32_t standard_version::
  major () const noexcept
  {
    std::uint64_t e (version % 10);
    std::uint64_t v (version / 10);
    std::uint64_t ab (v % 1000);
    if (ab != 0 || e == 1)
      v += 1000 - ab;

    //                                 AAAAABBBBBCCCCCDDD
    return static_cast<std::uint32_t> (v / 10000000000000 % 100000);
  }

  inline std::uint32_t standard_version::
  minor () const noexcept
  {
    std::uint64_t e (version % 10);
    std::uint64_t v (version / 10);
    std::uint64_t ab (v % 1000);
    if (ab != 0 || e == 1)
      v += 1000 - ab;

    //                            AAAAABBBBBCCCCCDDD
    return static_cast<std::uint32_t> (v / 100000000 % 100000);
  }

  inline std::uint32_t standard_version::
  patch () const noexcept
  {
    std::uint64_t e (version % 10);
    std::uint64_t v (version / 10);
    std::uint64_t ab (v % 1000);
    if (ab != 0 || e == 1)
      v += 1000 - ab;

    //                       AAAAABBBBBCCCCCDDD
    return static_cast<std::uint32_t> (v / 1000 % 100000);
  }

  inline bool standard_version::
  release () const noexcept
  {
    return version % 10000 == 0;
  }

  inline optional<std::uint16_t> standard_version::
  pre_release () const noexcept
  {
    return release () || stub ()
      ? nullopt
      : optional<std::uint16_t> (
          static_cast<std::uint16_t> (version / 10 % 1000));
  }

  inline optional<std::uint16_t> standard_version::
  alpha () const noexcept
  {
    optional<std::uint16_t> pr (pre_release ());
    return pr && *pr < 500 ? pr : nullopt;
  }

  inline optional<std::uint16_t> standard_version::
  beta () const noexcept
  {
    optional<std::uint16_t> pr (pre_release ());
    return pr && *pr >= 500 ? optional<std::uint16_t> (*pr - 500) : nullopt;
  }

  inline bool standard_version::
  final () const noexcept
  {
    return release () || !(snapshot () || stub ());
  }

  inline bool standard_version::
  earliest () const noexcept
  {
    return version % 10000 == 1 && !snapshot () && !stub ();
  }

  inline bool standard_version::
  latest_snapshot () const noexcept
  {
    return snapshot () && snapshot_sn == latest_sn;
  }

  // Note: in the following constructors we subtract one from AAAAABBBBBCCCCC
  // if DDDE is not zero (see standard-version.hxx for details).
  //
  inline standard_version::
  standard_version (std::uint16_t ep,
                    std::uint32_t mj,
                    std::uint32_t mi,
                    std::uint32_t pa,
                    std::uint16_t pr,
                    std::uint16_t rv)
      : standard_version (ep,
                          // AAAAABBBBBCCCCCDDDE
                          (mj *  100000000000000ULL +
                           mi *       1000000000ULL +
                           pa *            10000ULL +
                           pr *               10ULL -
                           (pr != 0 ?      10000ULL : 0ULL)),
                          "" /* snapshot */,
                          rv)
  {
  }

  inline standard_version::
  standard_version (std::uint16_t ep,
                    std::uint32_t mj,
                    std::uint32_t mi,
                    std::uint32_t pa,
                    std::uint16_t pr,
                    std::uint64_t sn,
                    std::string si,
                    std::uint16_t rv)
      : standard_version (ep,
                          // AAAAABBBBBCCCCCDDDE
                          (mj *  100000000000000ULL                +
                           mi *       1000000000ULL                +
                           pa *            10000ULL                +
                           pr *               10ULL                +
                                               1ULL /* snapshot */ -
                                           10000ULL),
                          sn,
                          si,
                          rv)
  {
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
