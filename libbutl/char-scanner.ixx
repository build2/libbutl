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

  inline auto char_scanner::
  peek_ () -> int_type
  {
    if (gptr_ != egptr_)
      return *gptr_;

    int_type r (is_.peek ());

    // Update buffer pointers for the next chunk.
    //
    if (buf_ != nullptr)
    {
      gptr_ = buf_->gptr ();
      egptr_ = buf_->egptr ();
    }

    return r;
  }

  inline void char_scanner::
  get_ ()
  {
    int_type c;

    if (gptr_ != egptr_)
    {
      buf_->gbump (1);
      c = *gptr_++;
    }
    else
      c = is_.get (); // About as fast as ignore() and way faster than tellg().

    if (save_ != nullptr && c != xchar::traits_type::eof ())
      save_->push_back (static_cast<char_type> (c));
  }
}
