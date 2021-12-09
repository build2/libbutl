// file      : libbutl/char-scanner.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  template <typename V, std::size_t N>
  inline char_scanner<V, N>::
  char_scanner (std::istream& is,
                bool crlf,
                std::uint64_t l, std::uint64_t c, std::uint64_t p)
      : char_scanner (is, validator_type (), crlf, l, c, p)
  {
  }

  template <typename V, std::size_t N>
  inline auto char_scanner<V, N>::
  peek (std::string& what) -> xchar
  {
    return peek (&what);
  }

  template <typename V, std::size_t N>
  inline auto char_scanner<V, N>::
  peek () -> xchar
  {
    return peek (nullptr /* what */);
  }

  template <typename V, std::size_t N>
  inline auto char_scanner<V, N>::
  get (std::string* what) -> xchar
  {
    if (ungetn_ != 0)
      return ungetb_[--ungetn_];
    else
    {
      xchar c (peek (what));
      get (c);
      return c;
    }
  }

  template <typename V, std::size_t N>
  inline auto char_scanner<V, N>::
  get (std::string& what) -> xchar
  {
    return get (&what);
  }

  template <typename V, std::size_t N>
  inline auto char_scanner<V, N>::
  get () -> xchar
  {
    return get (nullptr /* what */);
  }

  template <typename V, std::size_t N>
  inline void char_scanner<V, N>::
  unget (const xchar& c)
  {
    // Because iostream::unget cannot work once eos is reached, we have to
    // provide our own implementation.
    //
    assert (ungetn_ != N); // Make sure the buffer is not filled.

    ungetb_[ungetn_++] = c;
  }

  template <typename V, std::size_t N>
  inline auto char_scanner<V, N>::
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

  template <typename V, std::size_t N>
  inline void char_scanner<V, N>::
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

  template <typename V, std::size_t N>
  inline std::uint64_t char_scanner<V, N>::
  pos_ () const
  {
    return buf_ != nullptr ? buf_->tellg () : 0;
  }
}
