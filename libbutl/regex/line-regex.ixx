namespace butl
{
  namespace regex
  {
    namespace details
    {
      inline char_regex::flag_type
      to_std_flags (char_flags f)
      {
        // Note that ECMAScript flag is implied in the absense of a grammar
        // flag.
        //
        return (f & char_flags::icase) != char_flags::none
               ? char_regex::icase
               : char_regex::flag_type ();
      }

      LIBBUTL_SYMEXPORT std::string
      transform_regex (const std::string&, char_flags);
    }

    // char_regex
    //
    inline char_regex::
    char_regex (const char_string& s, char_flags f)
        : base_type (((f & char_flags::idot) != char_flags::none
                      ? details::transform_regex (s, f)
                      : s),
                     details::to_std_flags (f))
    {
    }

    // char_flags
    //
    inline char_flags
    operator&= (char_flags& x, char_flags y)
    {
      return x = static_cast<char_flags> (
        static_cast<uint16_t> (x) & static_cast<uint16_t> (y));
    }

    inline char_flags
    operator|= (char_flags& x, char_flags y)
    {
      return x = static_cast<char_flags> (
        static_cast<uint16_t> (x) | static_cast<uint16_t> (y));
    }

    inline char_flags
    operator& (char_flags x, char_flags y) {return x &= y;}

    inline char_flags
    operator| (char_flags x, char_flags y) {return x |= y;}
  }
}
