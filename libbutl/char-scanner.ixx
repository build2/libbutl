// file      : libbutl/char-scanner.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline auto char_scanner::
  get () -> xchar
  {
    if (unget_)
    {
      unget_ = false;
      return ungetc_;
    }
    else
    {
      xchar c (peek ());
      get (c);
      return c;
    }
  }

  inline void char_scanner::
  unget (const xchar& c)
  {
    // Because iostream::unget cannot work once eos is reached, we have to
    // provide our own implementation.
    //
    unget_ = true;
    ungetc_ = c;
  }
}
