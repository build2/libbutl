// file      : libbutl/char-scanner.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_CHAR_SCANNER_HXX
#define LIBBUTL_CHAR_SCANNER_HXX

#include <string>  // char_traits
#include <cstdint> // uint64_t
#include <istream>

#include <libbutl/fdstream.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // Low-level character stream scanner. Normally used as a base for
  // higher-level lexers.
  //
  class LIBBUTL_EXPORT char_scanner
  {
  public:
    // If the crlf argument is true, then recognize Windows newlines (0x0D
    // 0x0A) and convert them to just '\n' (0x0A). Note that a standalone
    // 0x0D is treated "as if" it was followed by 0x0A.
    //
    // Note also that if the stream happens to be ifdstream, then it includes
    // a number of optimizations that assume nobody else is messing with the
    // stream.
    //
    char_scanner (std::istream& is, bool crlf = true);

    char_scanner (const char_scanner&) = delete;
    char_scanner& operator= (const char_scanner&) = delete;

    // Scanner interface.
    //
  public:

    // Extended character. It includes line/column information and is capable
    // of representing EOF.
    //
    // Note that implicit conversion of EOF to char_type results in NUL
    // character (which means in most cases it is safe to compare xchar to
    // char without checking for EOF).
    //
    class xchar
    {
    public:
      using traits_type = std::char_traits<char>;
      using int_type = traits_type::int_type;
      using char_type = traits_type::char_type;

      int_type value;
      std::uint64_t line;
      std::uint64_t column;

      operator char_type () const
      {
        return value != traits_type::eof ()
          ? static_cast<char_type> (value)
          : char_type (0);
      }

      xchar (int_type v, std::uint64_t l = 0, std::uint64_t c = 0)
          : value (v), line (l), column (c) {}
    };

    xchar
    get ();

    void
    get (const xchar& peeked); // Get previously peeked character (faster).

    void
    unget (const xchar&);

    // Note that if there is an "ungot" character, peek() will return
    // that.
    //
    xchar
    peek ();

    // Tests. In the future we can add tests line alpha(), alnum(),
    // etc.
    //
    static bool
    eos (const xchar& c) {return c.value == xchar::traits_type::eof ();}

    // Line and column of the next character to be extracted from the stream
    // by peek() or get().
    //
    std::uint64_t line = 1;
    std::uint64_t column = 1;

  protected:
    using int_type = xchar::int_type;
    using char_type = xchar::char_type;

    int_type
    peek_ ();

    void
    get_ ();

  protected:
    std::istream& is_;

    fdbuf* buf_; // NULL if not ifdstream.
    const char_type* gptr_;
    const char_type* egptr_;

    bool crlf_;
    bool eos_ = false;

    bool unget_ = false;
    bool unpeek_ = false;

    xchar ungetc_ = '\0';
    xchar unpeekc_ = '\0';
  };
}

#include <libbutl/char-scanner.ixx>

#endif // LIBBUTL_CHAR_SCANNER_HXX
