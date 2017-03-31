// file      : butl/tab-parser.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/tab-parser>

#include <cassert>
#include <sstream>

using namespace std;

namespace butl
{
  using parsing = tab_parsing;

  // tab_parser
  //
  tab_fields tab_parser::
  next ()
  {
    tab_fields r;
    xchar c (skip_spaces ()); // Skip empty lines and leading spaces.

    auto eol   = [&c] () -> bool {return eos (c) || c == '\n';};
    auto space = [&c] () -> bool {return c == ' ' || c == '\t';};
    auto next  = [&c, this] () {get (); c = peek ();};

    r.line = c.line;

    // Read line fields until eos or the newline character.
    //
    while (!eol ())
    {
      for (; !eol () && space (); next ()) ; // Skip space characters.

      if (eol ()) // No more fields.
        break;

      // Read the field. Here we scan until the first whitespace character that
      // appears out of quotes.
      //
      tab_field tf ({string (), c.column});
      char quoting ('\0'); // Current quoting mode, can be used as bool.

      for (; !eol (); next ())
      {
        if (!quoting)
        {
          if (space ())                   // End of the field.
            break;
          else if (c == '"' || c == '\'') // Begin of quoted string.
            quoting = c;
        }
        else if (c == quoting)            // End of quoted string.
          quoting = '\0';

        tf.value += c;
      }

      if (quoting)
        throw parsing (name_, c.line, c.column, "unterminated quoted string");

      r.emplace_back (move (tf));
    }

    r.end_column = c.column;

    // Read out eof or newline character from the stream. Note that "reading"
    // eof multiple times is safe.
    //
    get ();
    return r;
  }

  tab_parser::xchar tab_parser::
  skip_spaces ()
  {
    xchar c (peek ());
    bool start (c.column == 1);

    for (; !eos (c); c = peek ())
    {
      switch (c)
      {
      case ' ':
      case '\t':
        break;
      case '\n':
        {
          // Skip empty lines.
          //
          if (!start)
            return c;

          break;
        }
      case '#':
        {
          // We only recognize '#' as a start of a comment at the beginning
          // of the line (sans leading spaces).
          //
          if (!start)
            return c;

          get ();

          // Read until newline or eos.
          //
          for (c = peek (); !eos (c) && c != '\n'; c = peek ())
            get ();

          continue;
        }
      default:
        return c; // Not a space.
      }

      get ();
    }

    return c;
  }

  // tab_parsing
  //
  static string
  format (const string& n, uint64_t l, uint64_t c, const string& d)
  {
    ostringstream os;
    if (!n.empty ())
      os << n << ':';
    os << l << ':' << c << ": error: " << d;
    return os.str ();
  }

  tab_parsing::
  tab_parsing (const string& n, uint64_t l, uint64_t c, const string& d)
      : runtime_error (format (n, l, c, d)),
        name (n), line (l), column (c), description (d)
  {
  }
}
