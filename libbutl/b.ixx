// file      : libbutl/b.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  // b_info_flags
  //
  inline b_info_flags operator& (b_info_flags x, b_info_flags y)
  {
    return x &= y;
  }

  inline b_info_flags operator| (b_info_flags x, b_info_flags y)
  {
    return x |= y;
  }

  inline b_info_flags operator&= (b_info_flags& x, b_info_flags y)
  {
    return x = static_cast<b_info_flags> (
      static_cast<std::uint16_t> (x) &
      static_cast<std::uint16_t> (y));
  }

  inline b_info_flags operator|= (b_info_flags& x, b_info_flags y)
  {
    return x = static_cast<b_info_flags> (
      static_cast<std::uint16_t> (x) |
      static_cast<std::uint16_t> (y));
  }
}
