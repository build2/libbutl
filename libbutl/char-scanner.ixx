// file      : libbutl/char-scanner.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  template <typename V>
  inline char_scanner<V>::
  char_scanner (std::istream& is, bool crlf, std::uint64_t l, std::uint64_t p)
      : char_scanner (is, validator_type (), crlf, l, p)
  {
  }

  template <typename V>
  inline auto char_scanner<V>::
  peek (std::string& what) -> xchar
  {
    return peek (&what);
  }

  template <typename V>
  inline auto char_scanner<V>::
  peek () -> xchar
  {
    return peek (nullptr /* what */);
  }

  template <typename V>
  inline auto char_scanner<V>::
  get (std::string* what) -> xchar
  {
    if (unget_)
    {
      unget_ = false;
      return ungetc_;
    }
    else
    {
      xchar c (peek (what));
      get (c);
      return c;
    }
  }

  template <typename V>
  inline auto char_scanner<V>::
  get (std::string& what) -> xchar
  {
    return get (&what);
  }

  template <typename V>
  inline auto char_scanner<V>::
  get () -> xchar
  {
    return get (nullptr /* what */);
  }

  template <typename V>
  inline void char_scanner<V>::
  unget (const xchar& c)
  {
    // Because iostream::unget cannot work once eos is reached, we have to
    // provide our own implementation.
    //
    unget_ = true;
    ungetc_ = c;
  }

  template <typename V>
  inline auto char_scanner<V>::
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

  template <typename V>
  inline void char_scanner<V>::
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

    validated_ = false;

    if (save_ != nullptr && c != xchar::traits_type::eof ())
      save_->push_back (static_cast<char_type> (c));
  }

  template <typename V>
  inline std::uint64_t char_scanner<V>::
  pos_ () const
  {
    return buf_ != nullptr ? buf_->tellg () : 0;
  }
}
