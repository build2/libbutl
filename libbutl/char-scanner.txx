// file      : libbutl/char-scanner.txx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_lib_modules_ts
#include <utility> // move
#endif

namespace butl
{
  template <typename V>
  char_scanner<V>::
  char_scanner (std::istream& is,
                validator_type v,
                bool crlf,
                std::uint64_t l,
                std::uint64_t p)
      : line (l),
        column (1),
        position (p),
        is_ (is),
        val_ (std::move (v)),
        buf_ (dynamic_cast<fdbuf*> (is.rdbuf ())),
        gptr_ (nullptr),
        egptr_ (nullptr),
        crlf_ (crlf)
  {
  }

  template <typename V>
  auto char_scanner<V>::
  peek (std::string* what) -> xchar
  {
    if (unget_)
      return ungetc_;

    if (unpeek_)
      return unpeekc_;

    if (eos_)
      return xchar (xchar::traits_type::eof (), line, column, position);

    int_type v (peek_ ());

    if (v == xchar::traits_type::eof ())
    {
      if (!decoded_)
      {
        if (what != nullptr)
          *what = "unexpected end of stream";

        v = xchar::invalid ();
      }

      eos_ = true;
    }
    else
    {
      auto valid = [what, this] (int_type v)
      {
        if (validated_)
          return true;

        char c (xchar::traits_type::to_char_type (v));
        std::pair<bool, bool> r (what != nullptr
                                 ? val_.validate (c, *what)
                                 : val_.validate (c));

        decoded_ = r.second;
        validated_ = true;
        return r.first;
      };

      if (!valid (v))
        v = xchar::invalid ();
      else if (crlf_ && v == '\r')
      {
        // Note that '\r' is a valid character (otherwise we won't be here),
        // so we don't validate it again below. We also postpone the
        // validation of the next non-'\r' character (except EOF) until the
        // next peek() call.
        //
        int_type v1;
        do
        {
          get_ ();       // Sets validated_ to false.
          v1 = peek_ ();
        }
        while (v1 == '\r');

        if (v1 != '\n')
        {
          // We need to make sure subsequent calls to peek() return newline.
          //
          unpeek_ = true;
          unpeekc_ = xchar ('\n', line, column, position);

          // Note that the previous character is decoded ('\r') and so EOF is
          // legitimate.
          //
          if (v1 == xchar::traits_type::eof ())
            eos_ = true;
        }

        v = '\n';
      }
    }

    return xchar (v, line, column, position);
  }

  template <typename V>
  void char_scanner<V>::
  get (const xchar& c)
  {
    if (unget_)
      unget_ = false;
    else
    {
      if (unpeek_)
      {
        unpeek_ = false;
      }
      // When is_.get () returns eof, the failbit is also set (stupid,
      // isn't?) which may trigger an exception. To work around this
      // we will call peek() first and only call get() if it is not
      // eof. But we can only call peek() on eof once; any subsequent
      // calls will spoil the failbit (even more stupid).
      //
      else if (!eos (c))
        get_ ();

      if (!eos (c))
      {
        if (c == '\n')
        {
          line++;
          column = 1;
        }
        else if (decoded_) // The character is the last in a sequence?
          column++;

        position = pos_ ();
      }
    }
  }
}
