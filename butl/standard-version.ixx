// file      : butl/standard-version.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline standard_version::
  standard_version ( std::uint16_t e,
                     std::uint64_t v,
                     const std::string& s,
                     std::uint16_t r)
      : standard_version (v, s)
  {
    // Can't initialize above due to ctor delegating.
    //
    epoch = e;
    revision = r;
  }

  inline std::uint16_t standard_version::
  major () const noexcept
  {
    std::uint64_t v (version / 10);
    std::uint64_t ab (v % 1000);
    if (ab != 0)
      v += 1000 - ab;

    return static_cast<std::uint16_t> (v / 1000000000 % 1000);
  }

  inline std::uint16_t standard_version::
  minor () const noexcept
  {
    std::uint64_t v (version / 10);
    std::uint64_t ab (v % 1000);
    if (ab != 0)
      v += 1000 - ab;

    return static_cast<std::uint16_t> (v / 1000000 % 1000);
  }

  inline std::uint16_t standard_version::
  patch () const noexcept
  {
    std::uint64_t v (version / 10);
    std::uint64_t ab (v % 1000);
    if (ab != 0)
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
    return abe > 0 && abe < 5000;
  }

  inline bool standard_version::
  beta () const noexcept
  {
    std::uint64_t abe (version % 10000);
    return abe > 5000;
  }

  inline int standard_version::
  compare (const standard_version& v) const noexcept
  {
    if (epoch != v.epoch)
      return epoch < v.epoch ? -1 : 1;

    if (version != v.version)
      return version < v.version ? -1 : 1;

    if (snapshot_sn != v.snapshot_sn)
      return snapshot_sn < v.snapshot_sn ? -1 : 1;

    if (revision != v.revision)
      return revision < v.revision ? -1 : 1;

    return 0;
  }
}
