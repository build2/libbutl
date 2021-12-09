// file      : libbutl/char-scanner.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>  // char_traits
#include <cassert>
#include <cstddef> // size_t
#include <cstdint> // uint64_t
#include <climits> // INT_*
#include <utility> // pair, make_pair()
#include <istream>

#include <libbutl/bufstreambuf.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // Refer to utf8_validator for details.
  //
  struct noop_validator
  {
    std::pair<bool, bool>
    validate (char) {return std::make_pair (true, true);}

    std::pair<bool, bool>
    validate (char c, std::string&) {return validate (c);}
  };

  // Low-level character stream scanner. Normally used as a base for
  // higher-level lexers.
  //
  template <typename V = noop_validator, std::size_t N = 1>
  class char_scanner
  {
  public:
    using validator_type = V;
    static constexpr const std::size_t unget_depth = N;

    // If the crlf argument is true, then recognize Windows newlines (0x0D
    // 0x0A) and convert them to just '\n' (0x0A). Note that a standalone
    // 0x0D is treated "as if" it was followed by 0x0A and multiple 0x0D
    // are treated as one.
    //
    // Note also that if the stream happens to be bufstreambuf-based, then it
    // includes a number of optimizations that assume nobody else is messing
    // with the stream.
    //
    // The line, column, and position arguments can be used to override the
    // start line, column, and position in the stream (useful when re-scanning
    // data saved with the save_* facility).
    //
    char_scanner (std::istream&,
                  bool crlf = true,
                  std::uint64_t line = 1,
                  std::uint64_t column = 1,
                  std::uint64_t position = 0);

    char_scanner (std::istream&,
                  validator_type,
                  bool crlf = true,
                  std::uint64_t line = 1,
                  std::uint64_t column = 1,
                  std::uint64_t position = 0);

    char_scanner (const char_scanner&) = delete;
    char_scanner& operator= (const char_scanner&) = delete;

    // Scanner interface.
    //
  public:

    // Extended character. It includes line/column/position information and is
    // capable of representing EOF and invalid characters.
    //
    // Note that implicit conversion of EOF/invalid to char_type results in
    // NUL character (which means in most cases it is safe to compare xchar to
    // char without checking for EOF).
    //
    class xchar
    {
    public:
      using traits_type = std::char_traits<char>;
      using int_type = traits_type::int_type;
      using char_type = traits_type::char_type;

      int_type value;

      // Note that the column is of the codepoint this byte belongs to.
      //
      std::uint64_t line;
      std::uint64_t column;

      // Logical character position (see bufstreambuf for details on the
      // logical part) if the scanned stream is bufstreambuf-based and always
      // zero otherwise.
      //
      std::uint64_t position;

      static int_type
      invalid () {return traits_type::eof () != INT_MIN ? INT_MIN : INT_MAX;}

      operator char_type () const
      {
        return value != traits_type::eof () && value != invalid ()
          ? static_cast<char_type> (value)
          : char_type (0);
      }

      xchar (int_type v = 0,
             std::uint64_t l = 0,
             std::uint64_t c = 0,
             std::uint64_t p = 0)
          : value (v), line (l), column (c), position (p) {}
    };

    // Note that if any of the get() or peek() functions return an invalid
    // character, then the scanning has failed and none of them should be
    // called again.

    xchar
    get ();

    // As above but in case of an invalid character also return the
    // description of why it is invalid.
    //
    xchar
    get (std::string& what);

    void
    get (const xchar& peeked); // Get previously peeked character (faster).

    void
    unget (const xchar&);

    // Note that if there is an "ungot" character, peek() will return that.
    //
    xchar
    peek ();

    // As above but in case of an invalid character also return the
    // description of why it is invalid.
    //
    xchar
    peek (std::string& what);

    // Tests. In the future we can add tests line alpha(), alnum(), etc.
    //
    static bool
    eos (const xchar& c) {return c.value == xchar::traits_type::eof ();}

    static bool
    invalid (const xchar& c) {return c.value == xchar::invalid ();}

    // Line, column and position of the next character to be extracted from
    // the stream by peek() or get().
    //
    std::uint64_t line;
    std::uint64_t column;
    std::uint64_t position;

    // Ability to save raw data as it is being scanned. Note that the
    // character is only saved when it is got, not peeked.
    //
  public:
    void
    save_start (std::string& b)
    {
      assert (save_ == nullptr);
      save_ = &b;
    }

    void
    save_stop ()
    {
      assert (save_ != nullptr);
      save_ = nullptr;
    }

    struct save_guard
    {
      explicit
      save_guard (char_scanner& s, std::string& b): s_ (&s) {s.save_start (b);}

      void
      stop () {if (s_ != nullptr) {s_->save_stop (); s_ = nullptr;}}

      ~save_guard () {stop ();}

    private:
      char_scanner* s_;
    };

  protected:
    using int_type  = typename xchar::int_type;
    using char_type = typename xchar::char_type;

    int_type
    peek_ ();

    void
    get_ ();

    std::uint64_t
    pos_ () const;

    xchar
    get (std::string* what);

    xchar
    peek (std::string* what);

  protected:
    std::istream& is_;

    validator_type val_;
    bool decoded_   = true;  // The peeked character is last byte of sequence.
    bool validated_ = false; // The peeked character has been validated.

    // Note that if you are reading from the buffer directly, then it is also
    // your responsibility to call the validator and save the data (see
    // save_*().
    //
    // Besides that, make sure that the peek() call preceding the scan is
    // followed by the get() call (see validated_, decoded_, and unpeek_ for
    // the hairy details; realistically, you would probably only direct-scan
    // ASCII fragments).
    //
    bufstreambuf* buf_; // NULL if not bufstreambuf-based.
    const char_type* gptr_;
    const char_type* egptr_;

    std::string* save_ = nullptr;

    bool crlf_;
    bool eos_ = false;

    std::size_t ungetn_ = 0;
    xchar ungetb_[N];

    bool unpeek_ = false;
    xchar unpeekc_ = '\0';
  };
}

#include <libbutl/char-scanner.ixx>
#include <libbutl/char-scanner.txx>
