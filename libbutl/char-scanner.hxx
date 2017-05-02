// file      : libbutl/char-scanner.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_CHAR_SCANNER_HXX
#define LIBBUTL_CHAR_SCANNER_HXX

#include <string>  // char_traits
#include <iosfwd>
#include <cstdint> // uint64_t

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
    char_scanner (std::istream& is, bool crlf = true)
        : is_ (is), crlf_ (crlf) {}

    char_scanner (const char_scanner&) = delete;
    char_scanner& operator= (const char_scanner&) = delete;

    // Scanner interface.
    //
  public:

    // Extended character. It includes line/column information
    // and is capable of representing EOF.
    //
    class xchar
    {
    public:
      typedef std::char_traits<char> traits_type;
      typedef traits_type::int_type int_type;
      typedef traits_type::char_type char_type;

      int_type value;
      std::uint64_t line;
      std::uint64_t column;

      operator char_type () const {return static_cast<char_type> (value);}

      xchar (int_type v, std::uint64_t l = 0, std::uint64_t c = 0)
          : value (v), line (l), column (c) {}
    };

    xchar
    get ();

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

    // Line and column of the furthest seen (either via get() or
    // peek()) character.
    //
    std::uint64_t line {1};
    std::uint64_t column {1};

  protected:
    std::istream& is_;
    bool crlf_;

    bool unget_ {false};
    xchar buf_ = '\0';
    bool eos_ {false};
  };
}

#endif // LIBBUTL_CHAR_SCANNER_HXX