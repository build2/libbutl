// file      : butl/manifest-parser.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/manifest-parser>

#include <cassert>
#include <sstream>

using namespace std;

namespace butl
{
  using parsing = manifest_parsing;
  using name_value = manifest_name_value;

  name_value manifest_parser::
  next ()
  {
    if (s_ == end)
      return name_value {"", "", line, column, line, column};

    xchar c (skip_spaces ());

    // Here is the problem: if we are in the 'body' state (that is,
    // we are parsing inside the manifest) and we see the special
    // empty name, then before returning the "start" pair for the
    // next manifest, we have to return the "end" pair. One way
    // would be to cache the "start" pair and return it on the
    // next call of next(). But that would require quite a bit
    // of extra logic. The alternative is to detect the beginning
    // of the empty name before parsing too far. This way, the
    // next call to next() will start parsing where we left of
    // and return the "start" pair naturally.
    //
    if (s_ == body && c == ':')
    {
      s_ = start;
      return name_value {"", "", c.line, c.column, c.line, c.column};
    }

    // Regardless of the state, what should come next is a name,
    // potentially the special empty one.
    //
    name_value r;
    parse_name (r);

    skip_spaces ();
    c = get ();

    if (eos (c))
    {
      // This is ok as long as the name is empty.
      //
      if (!r.name.empty ())
        throw parsing (name_, c.line, c.column, "':' expected after name");

      s_ = end;

      // The "end" pair.
      //
      r.value_line = r.name_line;
      r.value_column = r.name_column;
      return r;
    }

    if (c != ':')
      throw parsing (name_, c.line, c.column, "':' expected after name");

    skip_spaces ();
    parse_value (r);

    c = peek ();

    // The character after the value should be either a newline or eos.
    //
    assert (c == '\n' || eos (c));

    if (c == '\n')
      get ();

    // Now figure out whether what we've got makes sense, depending
    // on the state we are in.
    //
    if (s_ == start)
    {
      // Start of the (next) manifest. The first pair should be the
      // special empty name/format version.
      //
      if (!r.name.empty ())
        throw parsing (name_, r.name_line, r.name_column,
                       "format version pair expected");

      // The version value is only mandatory for the first manifest in
      // a sequence.
      //
      if (r.value.empty ())
      {
        if (version_.empty ())
          throw parsing (name_, r.value_line, r.value_column,
                         "format version value expected");
        r.value = version_;
      }
      else
      {
        version_ = r.value; // Update with the latest.

        if (version_ != "1")
          throw parsing (name_, r.value_line, r.value_column,
                         "unsupported format version " + version_);
      }

      s_ = body;
    }
    else
    {
      // Parsing the body of the manifest.
      //

      // Should have been handled by the special case above.
      //
      assert (!r.name.empty ());
    }

    return r;
  }

  void manifest_parser::
  parse_name (name_value& r)
  {
    xchar c (peek ());

    r.name_line = c.line;
    r.name_column = c.column;

    for (; !eos (c); c = peek ())
    {
      if (c == ':' || c == ' ' || c == '\t' || c == '\n')
        break;

      r.name += c;
      get ();
    }
  }

  void manifest_parser::
  parse_value (name_value& r)
  {
    xchar c (peek ());

    r.value_line = c.line;
    r.value_column = c.column;

    string& v (r.value);
    string::size_type n (0); // Size of last non-space character (simple mode).

    // Detect the multi-line mode introductor.
    //
    bool ml (false);
    if (c == '\\')
    {
      get ();
      xchar p (peek ());

      if (p == '\n')
      {
        get (); // Newline is not part of the value so skip it.
        c = peek ();
        ml = true;
      }
      else if (eos (p))
        ml = true;
      else
        unget (c);
    }

    // The nl flag signals that the preceding character was a "special
    // newline", that is, a newline that was part of the milti-line mode
    // introductor or an escape sequence.
    //
    for (bool nl (ml); !eos (c); c = peek ())
    {
      // Detect the special "\n\\\n" sequence. In the multi-line mode,
      // this is a "terminator". In the simple mode, this is a way to
      // specify a newline.
      //
      // The key idea here is this: if we "swallowed" any characters
      // (i.e., called get() without a matching unget()), then we
      // have to restart the loop in order to do all the tests for
      // the next character. Also, for this to work, we can only
      // add one character to v, which limits us to maximum three
      // characters look-ahead: one in v, one "ungot", and one
      // peeked.
      //
      // The first block handles the special sequence that starts with
      // a special newline. In multi-line mode, this is an "immediate
      // termination" where we "use" the newline from the introductor.
      // Note also that in the simple mode the special sequence can
      // only start with a special (i.e., escaped) newline.
      //
      if (nl)
      {
        nl = false;

        if (c == '\\')
        {
          get ();
          xchar c1 (peek ());

          if (c1 == '\n' || eos (c1))
          {
            if (ml)
              break;
            else
            {
              if (c1 == '\n')
                get ();

              v += '\n'; // Literal newline.
              n = v.size ();
              continue; // Restart from the next character.
            }
          }
          else
            unget (c); // Fall through.
        }
      }

      if (c == '\n')
      {
        if (ml)
        {
          get ();
          xchar c1 (peek ());

          if (c1 == '\\')
          {
            get ();
            xchar c2 (peek ());

            if (c2 == '\n' || eos (c2))
              break;
            else
            {
              v += '\n';
              unget (c1);
              continue; // Restart from c1 (slash).
            }
          }
          else
            unget (c); // Fall through.
        }
        else
          break; // Simple value terminator.
      }

      // Detect the newline escape sequence. The same look-ahead
      // approach as above.
      //
      if (c == '\\')
      {
        get ();
        xchar c1 (peek ());

        if (c1 == '\n' || eos (c1))
        {
          if (c1 == '\n')
          {
            get ();
            nl = true; // This is a special newline.
          }
          continue; // Restart from the next character.
        }
        else if (c1 == '\\')
        {
          get ();
          xchar c2 (peek ());

          if (c2 == '\n' || eos (c1))
          {
            v += '\\';
            n = v.size ();
            // Restart from c2 (newline/eos).
          }
          else
          {
            v += '\\';
            n = v.size ();
            unget (c1); // Restart from c1 (second slash).
          }

          continue;
        }
        else
          unget (c); // Fall through.
      }

      get ();
      v += c;

      if (!ml && c != ' ' && c != '\t')
        n = v.size ();
    }

    // Cut off trailing whitespaces.
    //
    if (!ml)
      v.resize (n);
  }

  manifest_parser::xchar manifest_parser::
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

  // manifest_parsing
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

  manifest_parsing::
  manifest_parsing (const string& n, uint64_t l, uint64_t c, const string& d)
      : runtime_error (format (n, l, c, d)),
        name (n), line (l), column (c), description (d)
  {
  }
}
