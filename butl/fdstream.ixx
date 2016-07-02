// file      : butl/fdstream.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline fdopen_mode operator& (fdopen_mode x, fdopen_mode y) {return x &= y;}
  inline fdopen_mode operator| (fdopen_mode x, fdopen_mode y) {return x |= y;}

  inline fdopen_mode
  operator&= (fdopen_mode& x, fdopen_mode y)
  {
    return x = static_cast<fdopen_mode> (
      static_cast<std::uint16_t> (x) &
      static_cast<std::uint16_t> (y));
  }

  inline fdopen_mode
  operator|= (fdopen_mode& x, fdopen_mode y)
  {
    return x = static_cast<fdopen_mode> (
      static_cast<std::uint16_t> (x) |
      static_cast<std::uint16_t> (y));
  }
}
