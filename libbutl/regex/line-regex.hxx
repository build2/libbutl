#pragma once

#include <list>
#include <string>  // basic_string, char_traits
#include <cctype>  // isdigit()
#include <locale>
#include <utility> // move()
#include <cstdint> // uintptr_t, uint16_t
#include <cstddef> // size_t
#include <cassert>

#include <libstud/regex/regex.hxx>
#include <libstud/regex/export.hxx>

namespace stud
{
  namespace regex
  {
    using char_string = std::basic_string<char>;

    enum class char_flags: std::uint16_t
    {
      icase = 0x1, // Case-insensitive match.
      idot  = 0x2, // Invert '.' escaping.

      none = 0
    };

    char_flags operator&  (char_flags,  char_flags);
    char_flags operator|  (char_flags,  char_flags);
    char_flags operator&= (char_flags&, char_flags);
    char_flags operator|= (char_flags&, char_flags);

    // Restricts valid standard flags to just {icase}, extends with custom
    // flags {idot}.
    //
    class char_regex: public basic_regex<char>
    {
    public:
      using base_type = basic_regex<char>;

      char_regex (const char_string&, char_flags = char_flags::none);
    };

    // Newlines are line separators and are not part of the line:
    //
    // line<newline>line<newline>
    //
    // Specifically, this means that a customary trailing newline creates a
    // trailing blank line.
    //
    // All characters can inter-compare (though there cannot be regex
    // characters in the output, only in line_regex).
    //
    // Note that we assume that line_regex and the input to regex_match() use
    // the same pool.
    //
    struct line_pool
    {
      // Note that we assume the pool can be moved without invalidating
      // pointers to any already pooled entities.
      //
      // Note that we used to use unordered_set for strings but (1) there is
      // no general expectation that we will have many identical strings and
      // (2) the number of strings is not expected to be large. So that felt
      // like an overkill and we now use a list with linear search.
      //
      // @@ Actually our experience of testing bpkg proves the opposite. In
      //    pkg-build.testscript we match verbose output streams containing
      //    potentially thousands of lines with regular expressions with
      //    hundreds of lines. Both literal and regex line chars can be
      //    repeated multiple times in the pool. Maybe we should reconsider.
      //
      std::list<char_string> strings;
      std::list<char_regex> regexes;
    };

    enum class line_type
    {
      special,
      literal,
      regex
    };

    struct LIBSTUD_REGEX_SYMEXPORT line_char
    {
      // Steal last two bits from the pointer to store the type.
      //
    private:
      std::uintptr_t data_;

    public:
      line_type
      type () const {return static_cast<line_type> (data_ & 0x3);}

      int
      special () const
      {
        // Stored as (shifted) int16_t. Perform steps reversed to those that
        // are described in the comment for the corresponding ctor. Note that
        // the intermediate cast to uint16_t is required to portably preserve
        // the -1 special character.
        //
        return static_cast<std::int16_t> (
          static_cast<std::uint16_t> (data_ >> 2));
      }

      const char_string*
      literal () const
      {
        // Note that 2 rightmost bits are used for packaging line_char
        // type. Read the comment for the corresponding ctor for details.
        //
        return reinterpret_cast<const char_string*> (
          data_ & ~std::uintptr_t (0x3));
      }

      const char_regex*
      regex () const
      {
        // Note that 2 rightmost bits are used for packaging line_char
        // type. Read the comment for the corresponding ctor for details.
        //
        return reinterpret_cast<const char_regex*> (
          data_ & ~std::uintptr_t (0x3));
      }

      static const line_char nul;
      static const line_char eof;

      // Note: creates an uninitialized value.
      //
      line_char () = default;

      // Create a special character. The argument value must be one of the
      // following ones:
      //
      // 0 (nul character)
      // -1 (EOF)
      // [()|.*+?{}\0123456789,=!] (syntax characters excluding [])
      //
      // Note that the constructor is implicit to allow basic_regex to
      // implicitly construct line_chars from special char literals (in
      // particular the basic_regex implementation appends them to some
      // internal line_string).
      //
      line_char (int);

      // Create a literal character.
      //
      // Don't copy string if already pooled.
      //
      explicit
      line_char (const char_string&, line_pool&);

      explicit
      line_char (char_string&&, line_pool&);

      explicit
      line_char (const char_string* s) // Assume already pooled.
        //
        // Steal two bits from the pointer to package line_char type. Assume
        // (and statically assert) that char_string address is a multiple of
        // four.
        //
        : data_ (reinterpret_cast <std::uintptr_t> (s) |
                   static_cast <std::uintptr_t> (line_type::literal)) {}

      // Create a regex character.
      //
      explicit
      line_char (char_regex, line_pool&);

      explicit
      line_char (const char_regex* r) // Assume already pooled.
        //
        // Steal two bits from the pointer to package line_char type.  Assume
        // (and statically assert) that char_regex address is a multiple of
        // four.
        //
        : data_ (reinterpret_cast <std::uintptr_t> (r) |
                   static_cast <std::uintptr_t> (line_type::regex)) {}

      // Provide basic_regex with the ability to use line_char in a context
      // where a char value is expected (e.g., as a switch() expression). For
      // special line_chars return the stored special character. For other
      // character types the only reasonable strategy is to return a value
      // that differs from any other that can be encountered in a regex
      // expression and so will unlikelly be misinterpreted.
      //
      operator char () const
      {
        return type () == line_type::special ? special () : '\a'; // BELL.
      }

      // Return true if the character is a syntax (special) one.
      //
      static bool
      syntax (char);
    };

    // Perform "deep" characters comparison (for example match literal
    // character with a regex character), rather than just compare them
    // literally. At least one argument must be of a type other than regex as
    // there is no operator==() defined to compare regexes. Characters of the
    // literal type must share the same pool (strings are compared by pointers
    // not by values).
    //
    LIBSTUD_REGEX_SYMEXPORT bool
    operator== (const line_char&, const line_char&);

    // Return false if arguments are equal (operator==() returns true).
    // Otherwise if types are different return the value implying that special
    // < literal < regex. If types are special or literal return the result of
    // the respective characters or strings comparison. At least one argument
    // must be of a type other than regex as there is no operator<() defined
    // to compare regexes.
    //
    // While not very natural operation for the class we have, we have to
    // provide some meaningfull semantics for such a comparison as it is
    // specified by the CharTraits requirements. While we could provide it
    // right in the regex_char_traits specialization, let's keep it here for
    // basic_regex implementations that potentially can compare line_chars as
    // they compare them with expressions of other types (see below).
    //
    LIBSTUD_REGEX_SYMEXPORT bool
    operator< (const line_char&, const line_char&);

    inline bool
    operator!= (const line_char& l, const line_char& r)
    {
      return !(l == r);
    }

    inline bool
    operator<= (const line_char& l, const line_char& r)
    {
      return l < r || l == r;
    }

    // Provide basic_regex with the ability to compare line_char to a syntax
    // character. In the absense of the following operators, such comparisons
    // would be ambigious (given that there are implicit conversions
    // int->line_char and line_char->char).
    //
    // Note that these == and < operators can succeed only for a line_char of
    // the special type. For other types they always return false. That in
    // particular leads to the following case:
    //
    // (lc != c) != (lc < c || c < lc).
    //
    // Note that we can not assert line_char is of the special type as
    // basic_regex may need the ability to check if arbitrary line_char
    // belongs to some special characters range (like ['0', '9']).
    //
    inline bool
    operator== (const line_char& l, char c)
    {
      return l.type () == line_type::special && l.special () == c;
    }

    inline bool
    operator== (char c, const line_char& l)
    {
      return l == c;
    }

    inline bool
    operator!= (const line_char& l, char c)
    {
      return !(l == c);
    }

    inline bool
    operator!= (char c, const line_char& l)
    {
      return !(l == c);
    }

    inline bool
    operator< (const line_char& l, char c)
    {
      return l.type () == line_type::special && l.special () < c;
    }

    inline bool
    operator< (char c, const line_char& l)
    {
      return l.type () == line_type::special && c < l.special ();
    }

    inline bool
    operator<= (const line_char& l, char c)
    {
      return l < c || l == c;
    }

    inline bool
    operator<= (char c, const line_char& l)
    {
      return c < l || c == l;
    }

    // Similar to the above, provide basic_regex with the ability to compare
    // line_char to a special non-syntax value (0, etc).
    //
    inline bool
    operator== (const line_char& l, int v)
    {
      return l.type () == line_type::special && l.special () == v;
    }

    inline bool
    operator!= (const line_char& l, int v)
    {
      return !(l == v);
    }

    template <>
    struct LIBSTUD_REGEX_SYMEXPORT regex_char_traits<line_char>
    {
      // CharTraits requirements.
      //
      using char_type  = line_char;
      using int_type   = char_type;
      using off_type   = std::char_traits<char>::off_type;
      using pos_type   = std::char_traits<char>::pos_type;
      using state_type = std::char_traits<char>::state_type;

      static void
      assign (char_type& c1, const char_type& c2) {c1 = c2;}

      static char_type*
      assign (char_type*, size_t, char_type);

      // Note that eq() and lt() are not constexpr (as required by C++11)
      // because == and < operators for char_type are not constexpr.
      //
      static bool
      eq (const char_type& l, const char_type& r) {return l == r;}

      static bool
      lt (const char_type& l, const char_type& r) {return l < r;}

      static char_type*
      move (char_type*, const char_type*, size_t);

      static char_type*
      copy (char_type*, const char_type*, size_t);

      static int
      compare (const char_type*, const char_type*, size_t);

      static size_t
      length (const char_type*);

      static const char_type*
      find (const char_type*, size_t, const char_type&);

      static constexpr char_type
      to_char_type (const int_type& c) {return c;}

      static constexpr int_type
      to_int_type (const char_type& c) {return int_type (c);}

      // Note that the following functions are not constexpr (as required by
      // C++11) because their return expressions are not constexpr.
      //
      static bool
      eq_int_type (const int_type& l, const int_type& r) {return l == r;}

      static int_type eof () {return char_type::eof;}

      static int_type
      not_eof (const int_type& c)
      {
        return c != char_type::eof ? c : char_type::nul;
      }

      // regex_char_traits<>
      //
      using string_type =
        std::basic_string<char_type, regex_char_traits<char_type>>;

      // Note: no notion of a newline character.
      //
      static bool
      is_newline (char_type)
      {
        return false;
      }
    };

    using line_string = regex_char_traits<line_char>::string_type;

    // "C" locale that has std::ctype<line_char> facet installed. Used in the
    // regex_traits<line_char> specialization (see below).
    //
    class LIBSTUD_REGEX_SYMEXPORT line_char_locale: public std::locale
    {
    public:
      // Create a copy of the C++ locale that implements the classic "C"
      // locale semantics and install the std::ctype<line_char> facet into it.
      //
      line_char_locale ();
    };
  }
}

// Standard template specializations for line_char that are required for the
// basic_regex<line_char> instantiation.
//
namespace std
{
  // std::ctype<> must be derived from both std::ctype_base and
  // std::locale::facet (the later supports ref-counting used by the
  // std::locale implementation internally).
  //
  // MSVC STL for some reason also derives std::ctype_base from
  // std::locale::facet which produces "already a base-class" warning and
  // effectivelly breaks the reference counting. So we derive from
  // std::ctype_base only in this case.
  //
  template <>
  class LIBSTUD_REGEX_SYMEXPORT ctype<stud::regex::line_char>:
    public ctype_base
#if (!defined(_MSVC_STL_VERSION) && !defined(_CPPLIB_VER)) || \
    ( defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION >= 150) // Not yet existing.
    , public locale::facet
#endif
  {
    // Used by the implementation only.
    //
    using line_type = stud::regex::line_type;

  public:
    using char_type = stud::regex::line_char;

    static locale::id id;


#if (!defined(_MSVC_STL_VERSION) && !defined(_CPPLIB_VER)) || \
    ( defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION >= 150) // Not yet existing.
    explicit
    ctype (size_t refs = 0): locale::facet (refs) {}
#else
    explicit
    ctype (size_t refs = 0): ctype_base (refs) {}
#endif

    // While unnecessary, let's keep for completeness.
    //
    virtual
    ~ctype () override = default;

    // The C++ standard requires the following functions to call their virtual
    // (protected) do_*() counterparts that provide the real implementations.
    // The only purpose for this indirection is to provide a user with the
    // ability to customize existing (standard) ctype facets. As we do not
    // provide such an ability, for simplicity we will omit the do_*()
    // functions and provide the implementations directly. This should be safe
    // as nobody except us could call those protected functions.
    //
    bool
    is (mask m, char_type c) const
    {
      return m ==
             (c.type () == line_type::special && c.special () > 0 &&
              isdigit (static_cast<unsigned char> (c.special ()))
              ? digit
              : 0);
    }

    const char_type*
    is (const char_type*, const char_type*, mask*) const;

    const char_type*
    scan_is (mask, const char_type*, const char_type*) const;

    const char_type*
    scan_not (mask, const char_type*, const char_type*) const;

    char_type
    toupper (char_type c) const {return c;}

    const char_type*
    toupper (char_type*, const char_type* e) const {return e;}

    char_type
    tolower (char_type c) const {return c;}

    const char_type*
    tolower (char_type*, const char_type* e) const {return e;}

    char_type
    widen (char c) const {return char_type (c);}

    const char*
    widen (const char*, const char*, char_type*) const;

    char
    narrow (char_type c, char def) const
    {
      return c.type () == line_type::special ? c.special () : def;
    }

    const char_type*
    narrow (const char_type*, const char_type*, char, char*) const;
  };
}

namespace stud
{
  namespace regex
  {
    // Since line_regex doesn't support case-insensitive matching and bracket
    // expressions, this specialization doesn't implement the related
    // functions (transform_primary(), etc), checking that they are not
    // called. For this reason, the imbued locale is never used by this
    // specialization itself, but can potentially be returned (for example, to
    // regex implementation) on request. Note, however, that the current regex
    // implementation, specialized for line_char, doesn't use this locale
    // either (not directly nor indirectly via regex_traits) for the same
    // reason.
    //
    template <>
    class LIBSTUD_REGEX_SYMEXPORT regex_traits<line_char>
    {
    public:
      using char_type       = line_char;
      using string_type     = line_string;
      using locale_type     = line_char_locale;
      using char_class_type = regex_traits<char>::char_class_type;

      regex_traits () = default; // Unnecessary but let's keep for completeness.

      static std::size_t
      length (const char_type* p) {return string_type::traits_type::length (p);}

      char_type
      translate (char_type c) const {return c;}

      // Case-insensitive matching is not supported by line_regex. So there is
      // no reason for the function to be called.
      //
      char_type
      translate_nocase (char_type c) const {assert (false); return c;}

      // Return a sort-key - the exact copy of [b, e).
      //
      template <typename I>
      string_type
      transform (I b, I e) const {return string_type (b, e);}

      // Return a case-insensitive sort-key. Case-insensitive matching and
      // equivalence classes are not supported by line_regex. So there is no
      // reason for the function to be called.
      //
      template <typename I>
      string_type
      transform_primary (I b, I e) const
      {
        assert (false);
        return string_type (b, e);
      }

      // POSIX regex grammar and collating elements (e.g., [.tilde.]) in
      // particular are not supported. So there is no reason for the function
      // to be called.
      //
      template <typename I>
      string_type
      lookup_collatename (I, I) const {assert (false); return string_type ();}

      // Character classes (e.g., [:lower:]) are not supported. So there is no
      // reason for the function to be called.
      //
      template <typename I>
      char_class_type
      lookup_classname (I, I, bool = false) const
      {
        assert (false);
        return char_class_type ();
      }

      // Return false as we don't support character classes (e.g., [:lower:]).
      //
      bool
      isctype (char_type, char_class_type) const {return false;}

      int
      value (char_type, int) const;

      // Return the locale passed as an argument as we do not expect anything
      // other than "C" (POSIX) locale, that we also assume to be imbued by
      // default.
      //
      locale_type
      imbue (locale_type l) {return l;}

      locale_type
      getloc () const {return locale_type ();}
    };

    class line_regex: public basic_regex<line_char>
    {
    public:
      using base_type = basic_regex<line_char>;

      using base_type::base_type;

      line_regex () = default;

      // Move string regex together with the pool used to create it.
      //
      line_regex (line_string&& s, line_pool&& p)
          // No move-string ctor for base_type, so emulate it.
          //
          : base_type (s), pool (std::move (p)) {s.clear ();}

      // Move constuctible/assignable-only type.
      //
      line_regex (line_regex&&) = default;
      line_regex (const line_regex&) = delete;
      line_regex& operator= (line_regex&&) = default;
      line_regex& operator= (const line_regex&) = delete;

    public:
      line_pool pool;
    };
  }
}

#include <libstud/regex/line-regex.ixx>
