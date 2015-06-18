// file      : butl/char-scanner.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2015 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/char-scanner>

#include <istream>

using namespace std;

namespace butl
{
  auto char_scanner::
  peek () -> xchar
  {
    if (unget_)
      return buf_;
    else
    {
      if (eos_)
        return xchar (xchar::traits_type::eof (), line, column);
      else
      {
        xchar::int_type v (is_.peek ());

        if (v == xchar::traits_type::eof ())
          eos_ = true;

        return xchar (v, line, column);
      }
    }
  }

  auto char_scanner::
  get () -> xchar
  {
    if (unget_)
    {
      unget_ = false;
      return buf_;
    }
    else
    {
      // When is_.get () returns eof, the failbit is also set (stupid,
      // isn't?) which may trigger an exception. To work around this
      // we will call peek() first and only call get() if it is not
      // eof. But we can only call peek() on eof once; any subsequent
      // calls will spoil the failbit (even more stupid).
      //
      xchar c (peek ());

      if (!eos (c))
      {
        is_.get ();

        if (c == '\n')
        {
          line++;
          column = 1;
        }
        else
          column++;
      }

      return c;
    }
  }

  void char_scanner::
  unget (const xchar& c)
  {
    // Because iostream::unget cannot work once eos is reached,
    // we have to provide our own implementation.
    //
    buf_ = c;
    unget_ = true;
  }
}
