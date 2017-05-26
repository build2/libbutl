// file      : libbutl/char-scanner.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <libbutl/char-scanner.hxx>

using namespace std;

namespace butl
{
  char_scanner::
  char_scanner (istream& is, bool crlf)
      : is_ (is),
        buf_ (dynamic_cast<fdbuf*> (is.rdbuf ())),
        gptr_ (nullptr),
        egptr_ (nullptr),
        crlf_ (crlf)
  {
  }

  auto char_scanner::
  peek () -> xchar
  {
    if (unget_)
      return ungetc_;

    if (unpeek_)
      return unpeekc_;

    if (eos_)
      return xchar (xchar::traits_type::eof (), line, column);

    int_type v (peek_ ());

    if (v == xchar::traits_type::eof ())
      eos_ = true;
    else if (crlf_ && v == 0x0D)
    {
      get_ ();
      int_type v1 (peek_ ());

      if (v1 != '\n')
      {
        // We need to make sure subsequent calls to peek() return newline.
        //
        unpeek_ = true;
        unpeekc_ = xchar ('\n', line, column);
      }

      v = '\n';
    }

    return xchar (v, line, column);
  }

  void char_scanner::
  get (const xchar& c)
  {
    if (unget_)
      unget_ = false;
    else if (unpeek_)
      unpeek_ = false;
    else
    {
      // When is_.get () returns eof, the failbit is also set (stupid,
      // isn't?) which may trigger an exception. To work around this
      // we will call peek() first and only call get() if it is not
      // eof. But we can only call peek() on eof once; any subsequent
      // calls will spoil the failbit (even more stupid).
      //
      if (!eos (c))
      {
        get_ ();

        if (c == '\n')
        {
          line++;
          column = 1;
        }
        else
          column++;
      }
    }
  }
}
