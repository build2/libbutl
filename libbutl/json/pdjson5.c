#ifndef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 200112L
#elif _POSIX_C_SOURCE < 200112L
#  error incompatible _POSIX_C_SOURCE level
#endif

#ifndef LIBPDJSON5_PDJSON5_H
#  include "pdjson5.h"
#endif

#include <stdlib.h>   // malloc()/realloc()/free()
#include <string.h>   // strlen()
#include <inttypes.h> // PR*

// Defaults.
//
#ifndef LIBPDJSON5_STACK_INC
#  define LIBPDJSON5_STACK_INC 16
#endif

#ifndef LIBPDJSON5_STACK_MAX
#  define LIBPDJSON5_STACK_MAX 1024
#endif

#ifndef LIBPDJSON5_VALUE_INIT
#  define LIBPDJSON5_VALUE_INIT 256
#endif

// Feature flags.
//
#define FLAG_STREAMING    0x01U
#define FLAG_JSON5        0x02U
#define FLAG_JSON5E       0x04U

// Runtime state flags.
//
#define FLAG_ERROR         0x08U
#define FLAG_NEWLINE       0x10U // Newline seen by last call to next().
#define FLAG_IMPLIED_END   0x20U // Implied top-level object end is pending.

#define json_error(json, format, ...)                             \
  if (!(json->flags & FLAG_ERROR))                                \
  {                                                               \
    snprintf (json->error_message, sizeof (json->error_message),  \
              format,                                             \
              __VA_ARGS__);                                       \
    json->flags |= FLAG_ERROR;                                    \
    json->subtype = PDJSON_ERROR_SYNTAX;                          \
  }

#define io_error(json, message)                                   \
  if (!(json->flags & FLAG_ERROR))                                \
  {                                                               \
    size_t n = sizeof (json->error_message) - 1;                  \
    strncpy (json->error_message, message, n);                    \
    json->error_message[n] = '\0';                                \
    json->flags |= FLAG_ERROR;                                    \
    json->subtype = PDJSON_ERROR_IO;                              \
  }

#define mem_error(json, message)                                  \
  if (!(json->flags & FLAG_ERROR))                                \
  {                                                               \
    size_t n = sizeof (json->error_message) - 1;                  \
    strncpy (json->error_message, message, n);                    \
    json->error_message[n] = '\0';                                \
    json->flags |= FLAG_ERROR;                                    \
    json->subtype = PDJSON_ERROR_MEMORY;                          \
  }

static size_t
utf8_seq_length (char byte)
{
  unsigned char u = (unsigned char) byte;
  if (u < 0x80)
    return 1;

  if (0x80 <= u && u <= 0xBF)
  {
    // second, third or fourth byte of a multi-byte
    // sequence, i.e. a "continuation byte"
    return 0;
  }
  else if (u == 0xC0 || u == 0xC1)
  {
    // overlong encoding of an ASCII byte
    return 0;
  }
  else if (0xC2 <= u && u <= 0xDF)
  {
    // 2-byte sequence
    return 2;
  }
  else if (0xE0 <= u && u <= 0xEF)
  {
    // 3-byte sequence
    return 3;
  }
  else if (0xF0 <= u && u <= 0xF4)
  {
    // 4-byte sequence
    return 4;
  }
  else
  {
    // u >= 0xF5
    // Restricted (start of 4-, 5- or 6-byte sequence) or invalid UTF-8
    return 0;
  }
}

static bool
is_legal_utf8 (const unsigned char *bytes, size_t length)
{
  if (0 == bytes || 0 == length)
    return false;

  unsigned char a;
  const unsigned char* srcptr = bytes + length;
  switch (length)
  {
  default:
    return false;
    // Everything else falls through when true.
  case 4:
    if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
    // Fall through.
  case 3:
    if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
    // Fall through.
  case 2:
    a = (*--srcptr);
    switch (*bytes)
    {
    case 0xE0:
      if (a < 0xA0 || a > 0xBF) return false;
      break;
    case 0xED:
      if (a < 0x80 || a > 0x9F) return false;
      break;
    case 0xF0:
      if (a < 0x90 || a > 0xBF) return false;
      break;
    case 0xF4:
      if (a < 0x80 || a > 0x8F) return false;
      break;
    default:
      if (a < 0x80 || a > 0xBF) return false;
      break;
    }
    // Fall through.
  case 1:
    if (*bytes >= 0x80 && *bytes < 0xC2)
      return false;
  }
  return *bytes <= 0xF4;
}


// See documentation for struct pdjson_user_io on reasonable assumptions
// around the io failure semantics.
//
// Checking for the io error after every call to peek()/get() is quite tedious
// and slow while io errors being fairly unlikely. As a result, we often use
// the following pattern:
//
// int c = source_get (json);
//
// if (c == EOF) // IOERROR
// {
//   json_error (...);
//   return PDJSON_ERROR;
// }
//
// The idea here is to piggy-back on the normal EOF handling (which in many
// contexts results in an error). The trick here is that json_error() is not
// going to override the error message if there is already a pending (io)
// error.
//
// Because of this implicit io error handing we annotate each call with the
// IOERROR comment to highlight where/how the io error is handled.
//
static int
source_peek_slow (pdjson_stream *json, struct pdjson_source *source)
{
  switch (source->tag)
  {
  case PDJSON_SOURCE_USER:
    {
      int c = source->source.user.io.peek (source->source.user.data);

      if (c == EOF                             &&
          source->source.user.io.error != NULL &&
          source->source.user.io.error (source->source.user.data))
        break;

      return c;
    }
  case PDJSON_SOURCE_STREAM:
    {
      FILE *stream = source->source.stream.stream;

      int c = getc (stream);
      if (c != EOF)
        c = ungetc (c, stream);

      if (c == EOF && ferror (stream))
        break;

      return c;
    }
  case PDJSON_SOURCE_BUFFER:
  case PDJSON_SOURCE_NULL:
    break;
  }

  io_error (json, "unable to read input text");
  return EOF;
}

static inline int
source_peek (pdjson_stream *json)
{
  struct pdjson_source *source = &json->source;

  return source->tag == PDJSON_SOURCE_BUFFER
    ? (source->position != source->source.buffer.length
       ? source->source.buffer.buffer[source->position]
       : EOF)
    : source_peek_slow (json, source);
}

static int
source_get_slow (pdjson_stream *json, struct pdjson_source *source)
{
  switch (source->tag)
  {
  case PDJSON_SOURCE_USER:
    {
      int c = source->source.user.io.get (source->source.user.data);

      if (c != EOF)
        source->position++;
      else if (source->source.user.io.error != NULL &&
               source->source.user.io.error (source->source.user.data))
        break;

      return c;
    }
  case PDJSON_SOURCE_STREAM:
    {
      FILE *stream = source->source.stream.stream;

      int c = getc (stream);

      if (c != EOF)
        source->position++;
      else if (ferror (stream))
        break;

      return c;
    }
  case PDJSON_SOURCE_BUFFER:
  case PDJSON_SOURCE_NULL:
    break;
  }

  io_error (json, "unable to read input text");
  return EOF;
}

static inline int
source_get (pdjson_stream *json)
{
  struct pdjson_source *source = &json->source;

  return source->tag == PDJSON_SOURCE_BUFFER
    ? (source->position != source->source.buffer.length
       ? source->source.buffer.buffer[source->position++]
       : EOF)
    : source_get_slow (json, source);
}

// Given the first byte of input or EOF (-1), read and decode the remaining
// bytes of a UTF-8 sequence (if any) and return its single-quoted UTF-8
// representation (e.g., "'A'") or, for control characters, its name (e.g.,
// "newline").
//
// Note: the passed character must be consumed, not peeked at (an exception
// can be made for EOF).
//
// See also read_space() for similar code.
//
// Note that this function may set FLAG_ERROR in case of an io error.
//
static const char *
diag_char (pdjson_stream *json, int c)
{
  if (c == EOF)
    return "end of text";

  switch (c)
  {
  case '\0': return "nul character";
  case '\b': return "backspace";
  case '\t': return "horizontal tab";
  case '\n': return "newline";
  case '\v': return "vertical tab";
  case '\f': return "form feed";
  case '\r': return "carrige return";
  default:
    if (c <= 31)
    {
      //snprintf (..., "control character %02lx", c);
      return "control character";
    }
  }

  size_t i = 0;
  char *s = json->utf8_char;

  s[i++] = '\'';
  s[i++] = c;

  if ((unsigned int)c >= 0x80)
  {
    size_t n = utf8_seq_length (c);
    if (!n)
      return "invalid UTF-8 sequence";

    size_t j;
    for (j = 1; j != n; ++j)
    {
      if ((c = source_get (json)) == EOF) // IOERROR
        break;

      s[i++] = c;
      json->lineadj++;
    }

    if (j != n || !is_legal_utf8 ((unsigned char*)s + 1, n)) // IOERROR
      return "invalid UTF-8 sequence";
  }

  s[i++] = '\'';
  s[i] = '\0';

  return s;
}

// As above but read the UTF-8 sequence from a string. Note: assumes valid
// UTF-8 and that the string doesn't end before the sequence.
//
static const char *
diag_char_string (pdjson_stream *json, const char* u)
{
  char c = *u;

  if ((unsigned char)c < 0x80)
    return diag_char (json, c);

  size_t i = 0;
  char *s = json->utf8_char;

  s[i++] = '\'';
  s[i++] = c;

  size_t n = utf8_seq_length (c);
  for (size_t j = 1; j < n; ++j)
    s[i++] = u[j];

  s[i++] = '\'';
  s[i] = '\0';

  return s;
}

// As above but for the decoded codepoint.
//
static const char *
diag_codepoint (pdjson_stream *json, uint32_t c)
{
  if (c == (uint32_t)-1 /* EOF */)
    return diag_char (json, EOF);

  if (c < 0x80)
    return diag_char (json, (int)c);

  size_t i = 0;
  char *s = json->utf8_char;

  s[i++] = '\'';

  if (c < 0x0800)
  {
    s[i++] = (c >> 6 & 0x1F) | 0xC0;
    s[i++] = (c >> 0 & 0x3F) | 0x80;
  }
  else if (c < 0x010000)
  {
    if (c >= 0xd800 && c <= 0xdfff)
      return "invalid codepoint";

    s[i++] = (c >> 12 & 0x0F) | 0xE0;
    s[i++] = (c >>  6 & 0x3F) | 0x80;
    s[i++] = (c >>  0 & 0x3F) | 0x80;
  }
  else if (c < 0x110000)
  {
    s[i++] = (c >> 18 & 0x07) | 0xF0;
    s[i++] = (c >> 12 & 0x3F) | 0x80;
    s[i++] = (c >> 6  & 0x3F) | 0x80;
    s[i++] = (c >> 0  & 0x3F) | 0x80;
  }
  else
    return "invalid codepoint";

  s[i++] = '\'';
  s[i] = '\0';

  return s;
}

struct pdjson_stack
{
  enum pdjson_type type;
  uint64_t count;
};

static enum pdjson_type
push (pdjson_stream *json, enum pdjson_type type)
{
  size_t new_stack_top = json->stack_top + 1;

#if LIBPDJSON5_STACK_MAX != 0
  if (new_stack_top > LIBPDJSON5_STACK_MAX)
  {
    json_error (json, "%s", "maximum depth of nesting reached");
    return PDJSON_ERROR;
  }
#endif

  if (new_stack_top >= json->stack_size)
  {
    size_t size = (json->stack_size + LIBPDJSON5_STACK_INC) *
      sizeof (struct pdjson_stack);

    struct pdjson_stack *stack = (struct pdjson_stack *)
      (json->alloc.malloc == NULL
       ? realloc (json->stack, size)
       : json->alloc.realloc (json->stack, size, json->alloc_data)); // THROW

    if (stack == NULL)
    {
      mem_error (json, "out of memory");
      return PDJSON_ERROR;
    }

    json->stack_size += LIBPDJSON5_STACK_INC;
    json->stack = stack;
  }

  json->stack[new_stack_top].type = type;
  json->stack[new_stack_top].count = 0;

  json->stack_top = new_stack_top;

  return type;
}

static enum pdjson_type
pop (pdjson_stream *json, enum pdjson_type type)
{
#if 0
  assert (json->stack != NULL &&
          json->stack[json->stack_top].type == (type == PDJSON_OBJECT_END
                                                ? PDJSON_OBJECT
                                                : PDJSON_ARRAY));
#endif

  json->stack_top--;
  return type;
}

static bool
pushchar (pdjson_stream *json, int c)
{
  if (json->data.string_fill == json->data.string_size)
  {
    size_t size = json->data.string_size * 2;

    char *buffer = (char *)
      (json->alloc.malloc == NULL
       ? realloc (json->data.string, size)
       : json->alloc.realloc (json->data.string, size, json->alloc_data)); // THROW

    if (buffer == NULL)
    {
      mem_error (json, "out of memory");
      return false;
    }

    json->data.string_size = size;
    json->data.string = buffer;
  }

  json->data.string[json->data.string_fill++] = c;

  return true;
}

// Match the remainder of input assuming the first character in pattern
// matched. If copy is true, also copy the remainder to the string buffer.
//
static enum pdjson_type
is_match (pdjson_stream *json,
          const char *pattern,
          bool copy,
          enum pdjson_type type)
{
  int c;
  for (const char *p = pattern + 1; *p; p++)
  {
    if ((c = source_get (json)) != *p) // IOERROR: *p cannot be EOF.
    {
      json_error (json,
                  "expected '%c' instead of %s in '%s'",
                  *p, diag_char (json, c), pattern);
      return PDJSON_ERROR;
    }

    if (copy)
    {
      if (!pushchar (json, c))
        return PDJSON_ERROR;
    }
  }

  if (copy)
  {
    if (!pushchar (json, '\0'))
      return PDJSON_ERROR;
  }

  return type;
}

// Match the remainder of the string buffer assuming the first character in
// the pattern matched.
//
static enum pdjson_type
is_match_string (pdjson_stream *json,
                 const char *pattern,
                 uint32_t nextcp,      // First codepoint after the string.
                 uint64_t* colno,      // Adjusted in case of an error.
                 enum pdjson_type type)
{
  const char *string = json->data.string + 1;

  size_t i = 0;
  for (int p; (p = pattern[i + 1]); i++)
  {
    int c = string[i];
    if (p != c)
    {
      if (c != '\0')
      {
        json_error (json,
                    "expected '%c' instead of %s in '%s'",
                    p, diag_char_string (json, string + i), pattern);
      }
      else
      {
        json_error (json,
                    "expected '%c' instead of %s in '%s'",
                    p, diag_codepoint (json, nextcp), pattern);
      }

      *colno += i;
      if (c != '\0' || nextcp != (uint32_t)-1)
        *colno += 1; // Plus 1 for the first character but minus 1 for EOF.

      return PDJSON_ERROR;
    }
  }

  if (string[i] != '\0')
  {
    json_error (json,
                "expected end of text instead of %s",
                diag_char_string (json, string + i));
    *colno += i + 1;
    return PDJSON_ERROR;
  }

  return type;
}

static bool
init_string (pdjson_stream *json)
{
  json->data.string_size = LIBPDJSON5_VALUE_INIT;

  json->data.string = (char *)
    (json->alloc.malloc == NULL
     ? malloc (json->data.string_size)
     : json->alloc.malloc (json->data.string_size, json->alloc_data)); // THROW

  if (json->data.string == NULL)
  {
    mem_error (json, "out of memory");
    return false;
  }
  return true;
}

static bool
encode_utf8 (pdjson_stream *json, uint32_t c)
{
  if (c < 0x80)
  {
    return pushchar (json, c);
  }
  else if (c < 0x0800)
  {
    return (pushchar (json, (c >> 6 & 0x1F) | 0xC0) &&
            pushchar (json, (c >> 0 & 0x3F) | 0x80));
  }
  else if (c < 0x010000)
  {
    if (c >= 0xD800 && c <= 0xDFFF)
    {
      json_error (json, "invalid codepoint U+%04" PRIX32, c);
      return false;
    }

    return (pushchar (json, (c >> 12 & 0x0F) | 0xE0) &&
            pushchar (json, (c >>  6 & 0x3F) | 0x80) &&
            pushchar (json, (c >>  0 & 0x3F) | 0x80));
  }
  else if (c < 0x110000)
  {
    return (pushchar (json, (c >> 18 & 0x07) | 0xF0) &&
            pushchar (json, (c >> 12 & 0x3F) | 0x80) &&
            pushchar (json, (c >> 6  & 0x3F) | 0x80) &&
            pushchar (json, (c >> 0  & 0x3F) | 0x80));
  }
  else
  {
    json_error (json, "unable to encode U+%04" PRIX32 " as UTF-8", c);
    return false;
  }
}

// Return a hex digit value or -1 if invalid.
//
static int
hexchar (int c)
{
  switch (c)
  {
  case '0': return 0;
  case '1': return 1;
  case '2': return 2;
  case '3': return 3;
  case '4': return 4;
  case '5': return 5;
  case '6': return 6;
  case '7': return 7;
  case '8': return 8;
  case '9': return 9;
  case 'a':
  case 'A': return 10;
  case 'b':
  case 'B': return 11;
  case 'c':
  case 'C': return 12;
  case 'd':
  case 'D': return 13;
  case 'e':
  case 'E': return 14;
  case 'f':
  case 'F': return 15;
  default:
    return -1;
  }
}

// Read 4-digit hex number in \uHHHH. Return (uint32_t)-1 if invalid.
//
static uint32_t
read_unicode_cp (pdjson_stream *json)
{
  uint32_t cp = 0;
  unsigned int shift = 12;

  for (size_t i = 0; i < 4; i++)
  {
    int hc;
    int c = source_get (json);

    if (c == EOF) // IOERROR
    {
      json_error (json, "%s", "unterminated string literal in Unicode escape");
      return (uint32_t)-1;
    }
    else if ((hc = hexchar (c)) == -1)
    {
      json_error (json,
                  "invalid Unicode escape hex digit %s",
                  diag_char (json, c));
      return (uint32_t)-1;
    }

    cp += (unsigned int)hc * (1U << shift);
    shift -= 4;
  }

  return cp;
}

static bool
read_unicode (pdjson_stream *json)
{
  uint32_t cp, h, l;

  if ((cp = read_unicode_cp (json)) == (uint32_t)-1)
    return false;

  if (cp >= 0xD800 && cp <= 0xDBFF)
  {
    // This is the high portion of a surrogate pair; we need to read the lower
    // portion to get the codepoint
    //
    h = cp;

    int c = source_get (json);
    if (c == EOF) // IOERROR
    {
      json_error (json, "%s", "unterminated string literal in Unicode");
      return false;
    }
    else if (c != '\\')
    {
      json_error (json,
                  "invalid surrogate pair continuation %s, expected '\\'",
                  diag_char (json, c));
      return false;
    }

    c = source_get (json);
    if (c == EOF) // IOERROR
    {
      json_error (json, "%s", "unterminated string literal in Unicode");
      return false;
    }
    else if (c != 'u')
    {
      json_error (json,
                  "invalid surrogate pair continuation %s, expected 'u'",
                  diag_char (json, c));
      return false;
    }

    if ((l = read_unicode_cp (json)) == (uint32_t)-1)
      return false;

    if (l < 0xDC00 || l > 0xDFFF)
    {
      json_error (json,
                  "surrogate pair continuation \\u%04" PRIX32
                  " out of DC00-DFFF range",
                  l);
      return false;
    }

    cp = ((h - 0xD800) * 0x400) + ((l - 0xDC00) + 0x10000);
  }
  else if (cp >= 0xDC00 && cp <= 0xDFFF)
  {
    json_error (json, "dangling surrogate \\u%04" PRIX32, cp);
    return false;
  }

  return encode_utf8 (json, cp);
}

// Read 4-digit hex number in \xHH. Return (uint32_t)-1 if invalid.
//
static uint32_t
read_latin_cp (pdjson_stream *json)
{
  uint32_t cp = 0;
  unsigned int shift = 4;

  for (size_t i = 0; i < 2; i++)
  {
    int hc;
    int c = source_get (json);

    if (c == EOF) // IOERROR
    {
      json_error (json, "%s", "unterminated string literal in Latin escape");
      return (uint32_t)-1;
    }
    else if ((hc = hexchar (c)) == -1)
    {
      json_error (json,
                  "invalid Latin escape hex digit %s",
                  diag_char (json, c));
      return (uint32_t)-1;
    }

    cp += (unsigned int)hc * (1U << shift);
    shift -= 4;
  }

  return cp;
}

static bool
read_latin (pdjson_stream *json)
{
  uint32_t cp;

  if ((cp = read_latin_cp (json)) == (uint32_t)-1)
    return false;

  return encode_utf8 (json, cp);
}

static bool
read_escaped (pdjson_stream *json)
{
  int c = source_get (json);
  if (c == EOF) // IOERROR
  {
    json_error (json, "%s", "unterminated string literal in escape");
    return false;
  }

  // JSON escapes.
  //

  if (c == 'u') // \xHHHH
    return read_unicode (json);

  int u = -1; // Unescaped character or -1 if invalid.
  switch (c)
  {
  case '\\': u = c;    break;
  case 'b':  u = '\b'; break;
  case 'f':  u = '\f'; break;
  case 'n':  u = '\n'; break;
  case 'r':  u = '\r'; break;
  case 't':  u = '\t'; break;
  case '/':  u = c;    break;
  case '"':  u = c;    break;
  }

  // Additional JSON5 escapes.
  //
  if (u == -1 && (json->flags & FLAG_JSON5))
  {
    if (c == 'x') // \xHH
      return read_latin (json);

    // According to the JSON5 spec (Section 5.1):
    //
    // "A decimal digit must not follow a reverse solidus followed by a
    // zero. [...] If any other character follows a reverse solidus, except
    // for the decimal digits 1 through 9, that character will be included in
    // the string, but the reverse solidus will not."
    //
    // So it appears:
    //
    // 1. \0N is not allowed.
    // 2. \N is not allowed either.
    // 3. Raw control characters can appear after `\`.
    //
    // The reference implementation appears to match this understanding.
    //
    switch (c)
    {
    case '\'': u = c;    break;
    case 'v':  u = '\v'; break;

    case '0':
      // Check that it's not followed by a digit (see above).
      //
      c = source_peek (json);
      if (c >= '0' && c <= '9')
        source_get (json); // Consume.
      else if (!(json->flags & FLAG_ERROR)) // IOERROR: u is -1.
        u = '\0';
      break;

      // Decimal digits (other that 0) are illegal (see above).
      //
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':  u = -1; break;

      // Line continuations.
      //
    case '\r':
      // Check if it's followed by \n (CRLF).
      //
      c = source_peek (json);
      if (c == '\n')
        source_get (json); // Consume.
      else if (json->flags & FLAG_ERROR) // IOERROR: u is -1.
        break;

      // Fall through.
    case '\n':
    case 0x2028:
    case 0x2029:
      return true; // No pushchar().

    default:
      // Pass as-is, including the control characters (see above).
      //
      u = c;
      break;
    }
  }

  if (u != -1)
    return pushchar (json, u);

  json_error (json, "invalid escape %s", diag_char (json, c));
  return false;
}

static bool
read_utf8 (pdjson_stream* json, int c)
{
  size_t n = utf8_seq_length (c);
  if (!n)
  {
    json_error (json, "%s", "invalid UTF-8 character");
    return false;
  }

  char buf[4];
  buf[0] = c;
  size_t i;
  for (i = 1; i < n; ++i)
  {
    if ((c = source_get (json)) == EOF) // IOERROR
      break;

    buf[i] = c;
    json->lineadj++;
  }

  if (i != n || !is_legal_utf8 ((unsigned char*)buf, n)) // IOERROR
  {
    json_error (json, "%s", "invalid UTF-8 text");
    return false;
  }

  for (i = 0; i < n; ++i)
  {
    if (!pushchar (json, buf[i]))
      return false;
  }

  return true;
}

static enum pdjson_type
read_string (pdjson_stream *json, int quote)
{
  if (json->data.string == NULL && !init_string (json))
    return PDJSON_ERROR;

  json->data.string_fill = 0;

  while (true)
  {
    int c = source_get (json);
    if (c == EOF) // IOERROR
    {
      json_error (json, "%s", "unterminated string literal");
      return PDJSON_ERROR;
    }
    else if (c == quote)
    {
      return pushchar (json, '\0') ? PDJSON_STRING : PDJSON_ERROR;
    }
    else if (c == '\\')
    {
      if (!read_escaped (json))
        return PDJSON_ERROR;
    }
    else if ((unsigned int) c >= 0x80)
    {
      if (!read_utf8 (json, c))
        return PDJSON_ERROR;
    }
    else
    {
      // According to the JSON5 spec (Chapter 5):
      //
      // "All Unicode characters may be placed within the quotation marks,
      // except for the characters that must be escaped: the quotation mark
      // used to begin and end the string, reverse solidus, and line
      // terminators."
      //
      // So it appears this includes the raw control characters (except
      // newlines). The reference implementation appears to match this
      // understanding.
      //
      // Note: quote and backslash are handled above.
      //
      if ((json->flags & FLAG_JSON5)
          ? (c == '\n' || c == '\r')
          : (c >= 0 && c < 0x20))
      {
        json_error (json, "%s", "unescaped control character in string");
        return PDJSON_ERROR;
      }

      if (!pushchar (json, c))
        return PDJSON_ERROR;
    }
  }

  return PDJSON_ERROR;
}

static inline bool
is_dec_digit (int c)
{
  return c >= '0' && c <= '9';
}

static bool
read_dec_digits (pdjson_stream *json)
{
  int c;
  size_t nread = 0;
  while (is_dec_digit (c = source_peek (json))) // IOERROR: not EOF.
  {
    source_get (json); // Consume.
    if (!pushchar (json, c))
      return false;

    nread++;
  }

  if (nread == 0)
  {
    source_get (json); // Consume.
    json_error (json,
                "expected digit instead of %s",
                diag_char (json, c));
  }

  return (json->flags & FLAG_ERROR) ? false : true; // IOERROR
}

static inline bool
is_hex_digit (int c)
{
  return ((c >= '0' && c <= '9') ||
          (c >= 'a' && c <= 'f') ||
          (c >= 'A' && c <= 'F'));
}

static bool
read_hex_digits (pdjson_stream *json)
{
  int c;
  size_t nread = 0;
  while (is_hex_digit (c = source_peek (json))) // IOERROR: not EOF.
  {
    source_get (json); // Consume.
    if (!pushchar (json, c))
      return false;

    nread++;
  }

  if (nread == 0)
  {
    source_get (json); // Consume.
    json_error (json, "expected hex digit instead of %s", diag_char (json, c));
  }

  return (json->flags & FLAG_ERROR) ? false : true; // IOERROR
}

// Given a consumed byte that starts a number, read the rest of it.
//
static enum pdjson_type
read_number (pdjson_stream *json, int c)
{
  if (json->data.string == NULL && !init_string (json))
    return PDJSON_ERROR;

  json->data.string_fill = 0;

  if (!pushchar (json, c))
    return PDJSON_ERROR;

  // Note: we can only have '+' here if we are in the JSON5 mode.
  //
  if (c == '-' || c == '+')
  {
    c = source_get (json);
    if (is_dec_digit (c) || // IOERROR: not EOF.
        ((json->flags & FLAG_JSON5) && (c == 'I' || c == 'N' || c == '.')))
    {
      if (!pushchar (json, c))
        return PDJSON_ERROR;

      // Fall through.
    }
    else // IOERROR
    {
      json_error (json, "unexpected %s in number", diag_char (json, c));
      return PDJSON_ERROR;
    }
  }

  if (c >= '1' && c <= '9')
  {
    c = source_peek (json);
    if (is_dec_digit (c)) // IOERROR: not EOF.
    {
      if (!read_dec_digits (json))
        return PDJSON_ERROR;
    }
    else if (json->flags & FLAG_ERROR) // IOERROR
      return PDJSON_ERROR;
  }
  else if (c == '0')
  {
    // Note that while the JSON5 spec doesn't say whether leading 0 is
    // illegal, the reference implementation appears to reject it. So we
    // assume it is (issue #58 in json5-spec).
    //
    c = source_peek (json);

    if (c == '.' ||  c == 'e' || c == 'E')
      ;
    else if ((json->flags & FLAG_JSON5) && (c == 'x' || c == 'X'))
    {
      source_get (json); // Consume.

      return (pushchar (json, c)     &&
              read_hex_digits (json) &&
              pushchar (json, '\0')) ? PDJSON_NUMBER : PDJSON_ERROR;
    }
    // There is a nuance: `01` in normal mode is two values.
    //
    else if (!(json->flags & FLAG_STREAMING) && is_dec_digit (c))
    {
      json_error (json, "%s", "leading '0' in number");
      return PDJSON_ERROR;
    }
    else if (json->flags & FLAG_ERROR) // IOERROR
      return PDJSON_ERROR;
  }
  // Note that we can only get `I`, `N`, and `.` here if we are in the JSON5
  // mode.
  //
  else if (c == 'I')
    return is_match (json, "Infinity", true /* copy */, PDJSON_NUMBER);
  else if (c == 'N')
    return is_match (json, "NaN", true /* copy */, PDJSON_NUMBER);
  else if (c == '.')
  {
    // It is more straightforward to handle leading dot as a special case. It
    // also takes care of the invalid sole dot case.
    //
    if (!read_dec_digits (json))
      return PDJSON_ERROR;

    c = source_peek (json);
    if (c != 'e' && c != 'E') // IOERROR
      return !(json->flags & FLAG_ERROR) && pushchar (json, '\0')
        ? PDJSON_NUMBER
        : PDJSON_ERROR;
  }

  // Up to decimal or exponent has been read.
  //
  c = source_peek (json);
  if (c != '.' && c != 'e' && c != 'E') // IOERROR
  {
    return !(json->flags & FLAG_ERROR) && pushchar (json, '\0')
      ? PDJSON_NUMBER
      : PDJSON_ERROR;
  }

  if (c == '.')
  {
    source_get (json); // Consume.

    if (!pushchar (json, c))
      return PDJSON_ERROR;

    if ((json->flags & FLAG_JSON5) &&
        !is_dec_digit (source_peek (json))) // IOERROR: subsequent peek/get.
      ; // Trailing dot.
    else if (!read_dec_digits (json))
      return PDJSON_ERROR;
  }

  // Check for exponent.
  //
  c = source_peek (json);
  if (c == 'e' || c == 'E')
  {
    source_get (json); // Consume.
    if (!pushchar (json, c))
      return PDJSON_ERROR;

    c = source_peek (json);
    if (c == '+' || c == '-')
    {
      source_get (json); // Consume.

      if (!pushchar (json, c))
        return PDJSON_ERROR;

      if (!read_dec_digits (json))
        return PDJSON_ERROR;
    }
    else if (is_dec_digit (c))
    {
      if (!read_dec_digits (json))
        return PDJSON_ERROR;
    }
    else // IOERROR
    {
      source_get (json); // Consume.
      json_error (json, "unexpected %s in number", diag_char (json, c));
      return PDJSON_ERROR;
    }
  }
  // else IOERROR

  return !(json->flags & FLAG_ERROR) && pushchar (json, '\0')
    ? PDJSON_NUMBER
    : PDJSON_ERROR;
}

static inline bool
is_space (const pdjson_stream *json, int c)
{
  switch (c)
  {
  case ' ':
  case '\n':
  case '\t':
  case '\r':
    return true;

    // See Chapter 8, "White Space" in the JSON5 spec.
    //
  case '\f':
  case '\v':
    return json->flags & FLAG_JSON5;

  default:
    return false;
  }
}

// Given the first byte (consumed), read and decode a multi-byte UTF-8
// sequence. Return true if it is a space, setting codepoint to the decoded
// value if not NULL. Trigger an error and return false if it's not.
//
static bool
read_space (pdjson_stream *json, int c, uint32_t* cp)
{
#if 0
  assert (c != EOF && (unsigned int)c >= 0x80);
#endif

  size_t i = 1;
  unsigned char *s = (unsigned char*)json->utf8_char;

  s[i++] = c;

  // See Chapter 8, "White Space" in the JSON5 spec.
  //
  // @@ TODO: handle Unicode Zs category.
  //
  // For now recognize the four JSON5E spaces ad hoc, without decoding
  // the sequence into the codepoint:
  //
  // U+00A0 - 0xC2 0xA0       (non-breaking space)
  // U+2028 - 0xE2 0x80 0xA8  (line seperator)
  // U+2029 - 0xE2 0x80 0xA9  (paragraph separator)
  // U+FEFF - 0xEF 0xBB 0xBF  (byte order marker)
  //
  size_t n = utf8_seq_length (c);

  const char* r = NULL;
  if (n != 0)
  {
    size_t j;
    for (j = 1; j != n; ++j)
    {
      if ((c = source_get (json)) == EOF) // IOERROR
        break;

      s[i++] = c;
      json->lineadj++;
    }

    if (j == n && is_legal_utf8 ((unsigned char*)s + 1, n))
    {
      if (n == 2 && s[1] == 0xC2 && s[2] == 0xA0)
      {
        if (cp != NULL) *cp = 0xA0;
      }
      else if (n == 3 &&
               s[1] == 0xE2 && s[2] == 0x80 && (s[3] == 0xA8 || s[3] == 0xA9))
      {
        if (cp != NULL) *cp = (s[3] == 0xA8 ? 0x2028 : 0x2029);
      }
      else if (n == 3 && s[1] == 0xEF && s[2] == 0xBB && s[3] == 0xBF)
      {
        if (cp != NULL) *cp = 0xFEFF;
      }
      else
      {
        s[0]   = '\'';
        s[i++] = '\'';
        s[i]   = '\0';
        r = (const char*)s;
      }
    }
    else // IOERROR
      r = "invalid UTF-8 sequence";
  }
  else
    r = "invalid UTF-8 sequence";

  if (r == NULL)
    return true;

  // Issuing diagnostics identical to the single-byte case would require
  // examining the context (are we inside an array, object, after name or
  // value inside the object, etc). See we keep it generic for now.
  //
  json_error (json, "unexpected Unicode character %s outside of string", r);
  return false;
}

static void
newline (pdjson_stream *json)
{
  json->lineno++;
  json->linepos = json->source.position;
  json->lineadj = 0;
  json->linecon = 0;
}

// Given the comment determinant character (`/`, `*`, `#`), skip everything
// until the end of the comment (newline or `*/`) and return the last
// character read (newline, '/', or EOF). If newline was seen, set
// FLAG_NEWLINE. This function can fail by returning EOF and setting the error
// flag.
//
static int
skip_comment (pdjson_stream *json, int c)
{
  switch (c)
  {
  case '/':
  case '#':
    {
      // Skip everything until the next newline or EOF.
      //
      while ((c = source_get (json)) != EOF) // IOERROR: return EOF/error flag.
      {
        if (c == '\n')
        {
          json->flags |= FLAG_NEWLINE;
          newline (json);
          break;
        }

        if (c == '\r')
          break;
      }

      break;
    }
  case '*':
    {
      // Skip everything until closing `*/` or EOF.
      //
      while ((c = source_get (json)) != EOF) // IOERROR: return EOF/error flag.
      {
        if (c == '*')
        {
          if (source_peek (json) == '/') // IOERROR: handled by above get().
          {
            c = source_get (json); // Consume closing `/`.
            break;
          }
        }
        else if (c == '\n')
        {
          json->flags |= FLAG_NEWLINE;
          newline (json);
        }
      }

      if (c == EOF)
        json_error (json, "%s", "unexpected end of text before '*/'");

      break;
    }
  }

  return c;
}

bool
pdjson_is_space (const pdjson_stream *json, int c)
{
  return is_space (json, c);
}

int
pdjson_skip_if_space (pdjson_stream *json, int c, uint32_t* cp)
{
  json->start_lineno = 0;
  json->start_colno = 0;

  if (c == EOF) // IOERROR
    return (json->flags & FLAG_ERROR) ? -1 : 0;

  if (is_space (json, c))
  {
    source_get (json); // Consume.

    if (c == '\n')
      newline (json);

    if (cp != NULL)
      *cp = (uint32_t)c;

    return 1;
  }

  if ((unsigned int)c >= 0x80)
  {
    source_get (json); // Consume.

    return read_space (json, c, cp) ? 1 : -1;
  }

  if ((c == '/' && (json->flags & FLAG_JSON5)) ||
      (c == '#' && (json->flags & FLAG_JSON5E)))
  {
    source_get (json); // Consume.

    uint64_t lineno = pdjson_get_line (json);
    uint64_t colno = pdjson_get_column (json);

    if (c == '/')
    {
      c = source_peek (json);

      if (c != '/' && c != '*') // IOERROR
      {
        // Have to diagnose here since consumed.
        //
        json_error (json, "%s", "unexpected '/'");
        return -1;
      }

      source_get (json); // Consume.
    }

    skip_comment (json, c);

    if (json->flags & FLAG_ERROR)
      return -1;

    // Point to the beginning of comment.
    //
    json->start_lineno = lineno;
    json->start_colno = colno;

    if (cp != NULL)
      *cp = (uint32_t)c;

    return 1;
  }

  return 0;
}

// Returns the next non-whitespace (and non-comment, for JSON5) character in
// the stream. If newline was seen, set FLAG_NEWLINE. This function can fail
// by returning EOF and setting the error flag.
//
// Note that this is the only function (besides the user-facing
// pdjson_source_get()) that needs to worry about newline housekeeping.
//
// Note also that we currently don't treat sole \r as a newline for the
// line/column counting purposes, even though JSON5 treats it as such (in
// comment end, line continuations). Doing that would require counting the
// \r\n sequence as a single newline. So while it can probably be done, we
// keep it simple for now.
//
// We will also require \n, not just \r, to be able to omit `,` in JSON5E.
//
static int
next (pdjson_stream *json)
{
  json->flags &= ~FLAG_NEWLINE;

  int c;
  while (true)
  {
    c = source_get (json); // IOERROR: return EOF/error flag.

    if (is_space (json, c))
    {
      if (c == '\n')
      {
        json->flags |= FLAG_NEWLINE;
        newline (json);
      }

      continue;
    }

    if (c != EOF && (unsigned int)c >= 0x80)
    {
      if (!read_space (json, c, NULL))
        return EOF; // Error is set.

      continue;
    }

    if ((c == '/' && (json->flags & FLAG_JSON5)) ||
        (c == '#' && (json->flags & FLAG_JSON5E)))
    {
      if (c == '/')
      {
        int p = source_peek (json); // IOERROR: subsequent peek/get.
        if (p == '/' || p == '*')
          c = source_get (json); // Consume.
        else
          break;
      }

      if ((c = skip_comment (json, c)) != EOF)
        continue;
    }

    break;
  }
  return c;
}

// The passed byte is expected to be consumed.
//
static enum pdjson_type
read_value (pdjson_stream *json, int c)
{
  uint64_t colno = pdjson_get_column (json);

  json->ntokens++;

  enum pdjson_type type = (enum pdjson_type)0;
  switch (c)
  {
  case EOF:
    json_error (json, "%s", "unexpected end of text");
    type = PDJSON_ERROR;
    break;
  case '{':
    type = push (json, PDJSON_OBJECT);
    break;
  case '[':
    type = push (json, PDJSON_ARRAY);
    break;
  case '\'':
    if (!(json->flags & FLAG_JSON5))
      break;
    // Fall through.
  case '"':
    type = read_string (json, c);
    break;
  case 'n':
    type = is_match (json, "null", false /* copy */, PDJSON_NULL);
    break;
  case 'f':
    type = is_match (json, "false", false /* copy */, PDJSON_FALSE);
    break;
  case 't':
    type = is_match (json, "true", false /* copy */, PDJSON_TRUE);
    break;
  case '+':
  case '.': // Leading dot
  case 'I': // Infinity
  case 'N': // NaN
    if (!(json->flags & FLAG_JSON5))
      break;
    // Fall through.
  case '-':
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    type = read_number (json, c);
    break;
  default:
    break;
  }

  if (type == 0)
  {
    json_error (json, "unexpected %s in value", diag_char (json, c));
    type = PDJSON_ERROR;
  }

  if (type != PDJSON_ERROR)
    json->start_colno = colno;

  return type;
}

// While the JSON5 spec says an identifier can be anything that matches the
// ECMAScript's IdentifierName production, this brings all kinds of Unicode
// complications (and allows `$` anywhere in the identifier). So for now we
// restrict it to the C identifier in the ASCII alphabet plus allow `$` (helps
// to pass reference implementation tests).
//
// For JSON5E we allow `-` and `.` but not as a first character. Both of these
// are valid beginnings of a JSON/JSON5 value (-1, .1) so strictly speaking,
// there is an ambiguity: is true-1 an identifier or two values (true and -1)?
// However, in our context (object member name), two values would be illegal.
// And so we resolve this ambiguity in favor of an identifier. One special
// case is the implied top-level object. But since implied objects are
// incompatible with the streaming mode, two top-level values would still be
// illegal (and, yes, true-1 is a valid two-value input in the streaming
// mode).
//
static inline bool
is_first_id_char (int c)
{
  return (c == '_'               ||
          (c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') ||
          c == '$');
}

static inline bool
is_subseq_id_char (int c, bool extended)
{
  return (c == '_'                             ||
          (c >= 'a' && c <= 'z')               ||
          (c >= 'A' && c <= 'Z')               ||
          (c >= '0' && c <= '9')               ||
          (extended && (c == '-' || c == '.')) ||
          c == '$');
}

// Read the remainder of an identifier given its first character.
//
static enum pdjson_type
read_identifier (pdjson_stream *json, int c)
{
  if (json->data.string == NULL && !init_string (json))
    return PDJSON_ERROR;

  json->data.string_fill = 0;

  for (bool extended = (json->flags & FLAG_JSON5E);;)
  {
    if (!pushchar (json, c))
      return PDJSON_ERROR;

    c = source_peek (json);

    if (!is_subseq_id_char (c, extended)) // IOERROR: not EOF.
      break;

    source_get (json); // Consume.
  }

  return !(json->flags & FLAG_ERROR) && pushchar (json, '\0') // IOERROR
    ? PDJSON_NAME
    : PDJSON_ERROR;
}

static enum pdjson_type
read_name (pdjson_stream *json, int c)
{
  uint64_t colno = pdjson_get_column (json);

  json->ntokens++;

  if (c == '"' || ((json->flags & FLAG_JSON5) && c == '\''))
  {
    if (read_string (json, c) == PDJSON_ERROR)
      return PDJSON_ERROR;
  }
  // See if this is an unquoted member name.
  //
  else if ((json->flags & FLAG_JSON5) && is_first_id_char (c))
  {
    if (read_identifier (json, c) == PDJSON_ERROR)
      return PDJSON_ERROR;
  }
  else
  {
    json_error (json, "%s", "expected member name");
    return PDJSON_ERROR;
  }

  json->start_colno = colno;

  return PDJSON_NAME;
}

enum pdjson_type
pdjson_peek (pdjson_stream *json)
{
  enum pdjson_type peek;
  if (json->peek)
    peek = json->peek;
  else
    peek = json->peek = pdjson_next (json);
  return peek;
}

enum pdjson_type
pdjson_next (pdjson_stream *json)
{
  if (json->flags & FLAG_ERROR)
    return PDJSON_ERROR;

  if (json->peek != 0)
  {
    enum pdjson_type next = json->peek;
    json->peek = (enum pdjson_type)0;
    return next;
  }

  if (json->pending.type != 0)
  {
    enum pdjson_type next = json->pending.type;
    json->pending.type = (enum pdjson_type)0;
    json->subtype = json->pending.subtype;
    json->start_lineno = json->pending.lineno;
    json->start_colno = json->pending.colno;

    if (next == PDJSON_OBJECT_END || next == PDJSON_ARRAY_END)
      next = pop (json, next);

    return next;
  }

  json->subtype = 0;
  json->start_lineno = 0;
  json->start_colno = 0;

  if (json->ntokens > 0 && json->stack_top == (size_t)-1)
  {
    // In the streaming mode leave any trailing whitespaces in the stream.
    // This allows the user to validate any desired separation between
    // values (such as newlines) using pdjson_source_get/peek() with any
    // remaining whitespaces ignored as leading when we parse the next
    // value.
    //
    if (!(json->flags & FLAG_STREAMING))
    {
      // If FLAG_IMPLIED_END is set here, then it means we have already seen
      // EOF.
      //
      if (!(json->flags & FLAG_IMPLIED_END))
      {
        int c = next (json);
        if (json->flags & FLAG_ERROR)
          return PDJSON_ERROR;

        if (c != EOF)
        {
          json_error (json,
                      "expected end of text instead of %s",
                      diag_char (json, c));
          return PDJSON_ERROR;
        }
      }
    }

    return PDJSON_DONE;
  }

  int c = next (json);
  if (json->flags & FLAG_ERROR)
    return PDJSON_ERROR;

  if (json->stack_top != (size_t)-1)
  {
    if (json->stack[json->stack_top].type == PDJSON_OBJECT)
    {
      if (json->stack[json->stack_top].count == 0)
      {
        // No member name/value pairs yet.
        //
        if (c == '}')
          return pop (json, PDJSON_OBJECT_END);

        json->stack[json->stack_top].count++;

        return read_name (json, c);
      }
      else if ((json->stack[json->stack_top].count % 2) == 0)
      {
        // Expecting comma followed by member name or closing brace.
        //
        // In JSON5 comma can be followed directly by the closing brace. And
        // in JSON5E it can also be followed by EOF in case of an implied
        // top-level object.
        //
        // In JSON5E comma can be omitted provided the preceding value and the
        // following name are separated by a newline. Or, to put it another
        // way, in this mode, if a newline was seen by the above call to
        // next() and the returned character is not '}' and, in the implied
        // case, not EOF, then we can rightfully expect a name.
        //
        bool implied = json->stack_top == 0 && (json->flags & FLAG_IMPLIED_END);
        if (c == ',')
        {
          c = next (json);
          if (json->flags & FLAG_ERROR)
            return PDJSON_ERROR;

          if (((json->flags & FLAG_JSON5) && c == '}') ||
              (implied && c == EOF))
            ; // Fall through.
          else
          {
            json->stack[json->stack_top].count++;
            return read_name (json, c);
          }
        }
        else if (json->flags & FLAG_JSON5E  &&
                 json->flags & FLAG_NEWLINE &&
                 c != '}' && (!implied || c != EOF))
        {
          json->stack[json->stack_top].count++;
          return read_name (json, c);
        }

        if (!implied)
        {
          if (c == '}')
            return pop (json, PDJSON_OBJECT_END);

          json_error (json,
                      "%s",
                      ((json->flags & FLAG_JSON5E)
                       ? "expected '}', newline, or ',' after member value"
                       : "expected ',' or '}' after member value"));
          return PDJSON_ERROR;
        }

        // Handle implied `}`.
        //
        if (c == EOF)
        {
          json->pending.type = PDJSON_DONE;
          json->pending.subtype = 0;
          json->pending.lineno = 0;
          json->pending.colno = 0;

          return pop (json, PDJSON_OBJECT_END);
        }

        if (c == '}')
        {
          json_error (json, "%s", "explicit '}' in implied object");
        }
        else
        {
          json_error (json, "%s", "expected newline or ',' after member value");
        }

        return PDJSON_ERROR;
      }
      else
      {
        // Expecting colon followed by value.
        //
        if (c == ':')
        {
          c = next (json);
          if (json->flags & FLAG_ERROR)
            return PDJSON_ERROR;

          json->stack[json->stack_top].count++;

          return read_value (json, c);
        }

        json_error (json, "%s", "expected ':' after member name");
        return PDJSON_ERROR;
      }
    }
    else
    {
#if 0
      assert (json->stack[json->stack_top].type == PDJSON_ARRAY);
#endif

      if (json->stack[json->stack_top].count == 0)
      {
        // No array values yet.
        //
        if (c == ']')
          return pop (json, PDJSON_ARRAY_END);

        json->stack[json->stack_top].count++;
        return read_value (json, c);
      }

      // Expecting comma followed by array value or closing brace.
      //
      // In JSON5 comma can be followed directly by the closing brace.
      //
      // In JSON5E comma can be omitted provided the preceding and the
      // following values are separated by a newline. Or, to put it another
      // way, in this mode, if a newline was seen by the above call to next()
      // and the returned character is not ']', then we can rightfully expect
      // a value.
      //
      if (c == ',')
      {
        c = next (json);
        if (json->flags & FLAG_ERROR)
          return PDJSON_ERROR;

        if ((json->flags & FLAG_JSON5) && c == ']')
          ; // Fall through.
        else
        {
          json->stack[json->stack_top].count++;
          return read_value (json, c);
        }
      }
      else if (json->flags & FLAG_JSON5E  &&
               json->flags & FLAG_NEWLINE &&
               c != ']')
      {
        json->stack[json->stack_top].count++;
        return read_value (json, c);
      }

      if (c == ']')
        return pop (json, PDJSON_ARRAY_END);

      json_error (json,
                  "%s",
                  ((json->flags & FLAG_JSON5E)
                   ? "expected ']', newline, or ',' after array value"
                   : "expected ',' or ']' after array value"));
      return PDJSON_ERROR;
    }
  }
  else
  {
    if (c == EOF && (json->flags & FLAG_STREAMING))
      return PDJSON_DONE;

    // Sniff out implied `{`.
    //
    // See below for the implied `}` injection.
    //
    // @@ TODO: move below to documentation.
    //
    // The object can be empty.
    //
    // Limitations:
    //
    // - Incompatible with the streaming mode.
    // - Line/columns numbers for implied `{` and `}` are of the first
    //   member name and EOF, respectively.
    //
    if ((json->flags & FLAG_JSON5E) &&
        !(json->flags & FLAG_STREAMING))
    {
      bool id;
      if ((id = is_first_id_char (c)) || c == '"' || c == '\'')
      {
        uint64_t lineno = pdjson_get_line (json);
        uint64_t colno = pdjson_get_column (json);

        json->ntokens++;

        if ((id
             ? read_identifier (json, c)
             : read_string (json, c)) == PDJSON_ERROR)
          return PDJSON_ERROR;

        enum pdjson_type type;

        // Peek at the next non-whitespace/comment character, similar to
        // next(). Note that skipping comments would require a two-character
        // look-ahead, which we don't have. However, `/` in this context that
        // does not start a comment would be illegal. So we simply diagnose
        // this case here, making sure to recreate exactly the same
        // diagnostics (both message and location-wise) as would be issued in
        // the non-extended mode.
        //
        // Save the first codepoint after the name as next codepoint for
        // diagnostics below.
        //
        // Note that this loop could probably be optimized at the expense of
        // readability (and it is already quite hairy). However, in common
        // cases we don't expect to make more than a few iterations.
        //
        uint32_t ncp;
        for (bool first = true; ; first = false)
        {
          c = source_peek (json);

          if (first)
          {
            if (c == EOF)                    ncp = (uint32_t)-1;
            else if ((unsigned int)c < 0x80) ncp = (uint32_t)c;
          }

          if (!is_space (json, c) && c != '/' && c != '#')
          {
            if (c == EOF || (unsigned int)c < 0x80) // IOERROR
              break;

            // Skip if whitespace or diagnose right away multi-byte UTF-8
            // sequence identical to the non-extended mode. Save decoded
            // codepoint if first.
            //
            source_get (json); // Consume;

            if (!read_space (json, c, first ? &ncp : NULL))
              return PDJSON_ERROR;

            continue;
          }

          source_get (json); // Consume.

          if (c == '\n')
            newline (json);

          else if (c == '/' || c == '#')
          {
            if (c == '/')
            {
              int p = source_peek (json);
              if (p == '/' || p == '*')
                c = source_get (json); // Consume.
              else // IOERROR
                break; // Diagnose consumed '/' below (or IOERROR).
            }

            if ((c = skip_comment (json, c)) == EOF)
            {
              if (json->flags & FLAG_ERROR)
                return PDJSON_ERROR;

              break;
            }
          }
        }

        if (c == ':')
        {
          json->pending.type = PDJSON_NAME;
          json->pending.subtype = 0;
          json->pending.lineno = lineno;
          json->pending.colno = colno;

          json->flags |= FLAG_IMPLIED_END;

          json->ntokens++; // For `{`.
          type = push (json, PDJSON_OBJECT);

          if (type != PDJSON_ERROR)
            json->stack[json->stack_top].count++; // For pending name.
        }
        else if (!(json->flags & FLAG_ERROR)) // IOERROR
        {
          // Return as a string or one of the literal values.
          //
          // Note that we have ambiguity between, for example, `true` and
          // `true_value`. But any continuation would be illegal so we resolve
          // it in favor of a member name. However, if not followed by `:`, we
          // need to diagnose identically to read_value () (both message and
          // position-wide), which gets a bit tricky.
          //
          if (id)
          {
            switch (json->data.string[0])
            {
            case 'n':
              type = is_match_string (json, "null", ncp , &colno, PDJSON_NULL);
              break;
            case 't':
              type = is_match_string (json, "true", ncp , &colno, PDJSON_TRUE);
              break;
            case 'f':
              type = is_match_string (json, "false", ncp , &colno, PDJSON_FALSE);
              break;
            case 'I':
              type = is_match_string (json, "Infinity", ncp , &colno, PDJSON_NUMBER);
              break;
            case 'N':
              type = is_match_string (json, "NaN", ncp , &colno, PDJSON_NUMBER);
              break;
            default:
              json_error (json,
                          "unexpected %s in value",
                          diag_char_string (json, json->data.string));
              type = PDJSON_ERROR;
            }
          }
          else
            type = PDJSON_STRING;

          // Per the above comment handling logic, if the character we are
          // looking at is `/`, then it is consumed, not peeked at, and so we
          // have to diagnose it here.
          //
          if (type != PDJSON_ERROR && c == '/')
          {
            json_error (json,
                        "expected end of text instead of %s",
                        diag_char (json, c));
            return PDJSON_ERROR; // Don't override location.
          }
        }
        else
          return PDJSON_ERROR; // IOERROR (don't override location).

        // Note: set even in case of an error since peek() above moved the
        // position past the name/value.
        //
        json->start_lineno = lineno;
        json->start_colno = colno;

        return type;
      }
      else if (c == EOF)
      {
        // Allow empty implied objects (for example, all members commented
        // out).
        //
        json->pending.type = PDJSON_OBJECT_END;
        json->pending.subtype = 0;
        json->pending.lineno = 0;
        json->pending.colno = 0;

        json->flags |= FLAG_IMPLIED_END;

        json->start_lineno = 1;
        json->start_colno = 1;

        // Note that we need to push an object entry into the stack to make
        // sure pdjson_get_context() works correctly.
        //
        json->ntokens++; // For `{`.
        return push (json, PDJSON_OBJECT);
      }
      // Else fall through.
    }

    return read_value (json, c);
  }
}

enum pdjson_type
pdjson_skip (pdjson_stream *json)
{
  enum pdjson_type type = pdjson_next (json);
  uint64_t cnt_arr = 0;
  uint64_t cnt_obj = 0;

  for (enum pdjson_type skip = type; ; skip = pdjson_next (json))
  {
    if (skip == PDJSON_ERROR || skip == PDJSON_DONE)
      return skip;

    if (skip == PDJSON_ARRAY)
      ++cnt_arr;
    else if (skip == PDJSON_ARRAY_END && cnt_arr > 0)
      --cnt_arr;
    else if (skip == PDJSON_OBJECT)
      ++cnt_obj;
    else if (skip == PDJSON_OBJECT_END && cnt_obj > 0)
      --cnt_obj;

    if (!cnt_arr && !cnt_obj)
      break;
  }

  return type;
}

enum pdjson_type
pdjson_skip_until (pdjson_stream *json, enum pdjson_type type)
{
  while (true)
  {
    enum pdjson_type skip = pdjson_skip (json);

    if (skip == PDJSON_ERROR || skip == PDJSON_DONE)
      return skip;

    if (skip == type)
      break;
  }

  return type;
}

enum pdjson_error_subtype
pdjson_get_error_subtype (const pdjson_stream *json)
{
  return (enum pdjson_error_subtype)json->subtype;
}

const char *
pdjson_get_name (const pdjson_stream *json, size_t *size)
{
  return pdjson_get_value (json, size);
}

const char *
pdjson_get_value (const pdjson_stream *json, size_t *size)
{
  if (size != NULL)
    *size = json->data.string_fill;

  if (json->data.string == NULL)
    return "";
  else
    return json->data.string;
}

const char *
pdjson_get_error (const pdjson_stream *json)
{
  return json->flags & FLAG_ERROR ? json->error_message : NULL;
}

uint64_t
pdjson_get_line (const pdjson_stream *json)
{
  return json->start_lineno == 0 ? json->lineno : json->start_lineno;
}

uint64_t
pdjson_get_column (const pdjson_stream *json)
{
  return json->start_colno == 0
    ? (json->source.position == 0
       ? 1
       : json->source.position - json->linepos - json->lineadj)
    : json->start_colno;
}

uint64_t
pdjson_get_position (const pdjson_stream *json)
{
  return json->source.position;
}

size_t
pdjson_get_depth (const pdjson_stream *json)
{
  return json->stack_top + 1;
}

enum pdjson_type
pdjson_get_context (const pdjson_stream *json, uint64_t *count)
{
  if (json->flags & FLAG_ERROR)
    return PDJSON_ERROR;

  if (json->stack_top == (size_t)-1)
    return PDJSON_DONE;

  if (count != NULL)
    *count = json->stack[json->stack_top].count;

  return json->stack[json->stack_top].type;
}

int
pdjson_source_get (pdjson_stream *json)
{
  // If the caller reads a multi-byte UTF-8 sequence, we expect them to read
  // it in its entirety. We also assume that any invalid bytes within such a
  // sequence belong to the same column (as opposed to starting a new column
  // or some such).
  //
  // In JSON5, if the caller starts reading a comment, we expect them to
  // finish reading it.

  json->flags &= ~FLAG_ERROR;

  int c = source_get (json); // IOERROR: return as EOF to caller.
  if (json->linecon != 0)
  {
    // Expecting a continuation byte within a multi-byte UTF-8 sequence.
    //
    if (c != EOF)
    {
      json->linecon--;
      json->lineadj++;
    }
  }
  else if (c == '\n')
    newline (json);
  else if (c >= 0xC2 && c <= 0xF4) // First in multi-byte UTF-8 sequence.
    json->linecon = utf8_seq_length (c) - 1;

  return c;
}

int
pdjson_source_peek (pdjson_stream *json)
{
  json->flags &= ~FLAG_ERROR;

  return source_peek (json); // IOERROR: return as EOF to caller.
}

bool
pdjson_source_error (pdjson_stream *json)
{
  return (json->flags & FLAG_ERROR) && (json->subtype & PDJSON_ERROR_IO);
}

void
pdjson_reset (pdjson_stream *json)
{
  json->start_lineno = 0;
  json->start_colno = 0;

  json->flags &= ~(FLAG_ERROR | FLAG_IMPLIED_END);
  json->ntokens = 0;
  json->subtype = 0;
  json->peek = (enum pdjson_type)0;
  json->pending.type = (enum pdjson_type)0;

  json->stack_top = (size_t)-1;
  json->data.string_fill = 0;

  json->error_message[0] = '\0';
}

static void
init (pdjson_stream *json, bool reinit)
{
  json->lineno = 1;
  json->linepos = 0;
  json->lineadj = 0;
  json->linecon = 0;
  json->start_lineno = 0;
  json->start_colno = 0;
  json->source.position = 0;

  json->flags &= reinit ? ~(FLAG_ERROR | FLAG_IMPLIED_END) : 0;
  json->ntokens = 0;
  json->subtype = 0;
  json->peek = (enum pdjson_type)0;
  json->pending.type = (enum pdjson_type)0;

  json->error_message[0] = '\0';

  json->stack_top = (size_t)-1;
  if (!reinit)
  {
    json->stack = NULL;
    json->stack_size = 0;
  }

  json->data.string_fill = 0;
  if (!reinit)
  {
    json->data.string = NULL;
    json->data.string_size = 0;
  }

  if (!reinit)
  {
    json->alloc.malloc = NULL;
    json->alloc.realloc = NULL;
    json->alloc.free = NULL;
    json->alloc_data = NULL;
  }
}

void
pdjson_open_null (pdjson_stream *json)
{
  init (json, false);
  json->source.tag = PDJSON_SOURCE_NULL;
}

void
pdjson_reopen_null (pdjson_stream *json)
{
  init (json, true);
  json->source.tag = PDJSON_SOURCE_NULL;
}

void
pdjson_open_buffer (pdjson_stream *json, const void *buffer, size_t size)
{
  init (json, false);
  json->source.tag = PDJSON_SOURCE_BUFFER;
  json->source.source.buffer.buffer = (const char *)buffer;
  json->source.source.buffer.length = size;
}

void
pdjson_reopen_buffer (pdjson_stream *json, const void *buffer, size_t size)
{
  init (json, true);
  json->source.tag = PDJSON_SOURCE_BUFFER;
  json->source.source.buffer.buffer = (const char *)buffer;
  json->source.source.buffer.length = size;
}

void
pdjson_open_string (pdjson_stream *json, const char *string)
{
  pdjson_open_buffer (json, string, strlen (string));
}

void
pdjson_reopen_string (pdjson_stream *json, const char *string)
{
  pdjson_reopen_buffer (json, string, strlen (string));
}

void
pdjson_open_stream (pdjson_stream *json, FILE *stream)
{
  init (json, false);
  json->source.tag = PDJSON_SOURCE_STREAM;
  json->source.source.stream.stream = stream;
}

void
pdjson_reopen_stream (pdjson_stream *json, FILE *stream)
{
  init (json, true);
  json->source.tag = PDJSON_SOURCE_STREAM;
  json->source.source.stream.stream = stream;
}

void
pdjson_open_user (pdjson_stream *json, const pdjson_user_io *io, void *data)
{
  init (json, false);
  json->source.tag = PDJSON_SOURCE_USER;
  json->source.source.user.data = data;
  json->source.source.user.io = *io;
}

void
pdjson_reopen_user (pdjson_stream *json, const pdjson_user_io *io, void *data)
{
  init (json, true);
  json->source.tag = PDJSON_SOURCE_USER;
  json->source.source.user.data = data;
  json->source.source.user.io = *io;
}

void
pdjson_set_allocator (pdjson_stream *json,
                      const pdjson_allocator *alloc,
                      void *data)
{
#if 0
  assert (json->stack == NULL && json->data.string == NULL);
#endif

  json->alloc = *alloc;
  json->alloc_data = data;
}

void
pdjson_set_streaming (pdjson_stream *json, bool mode)
{
  if (mode)
    json->flags |= FLAG_STREAMING;
  else
    json->flags &= ~FLAG_STREAMING;
}

void
pdjson_set_language (pdjson_stream *json, enum pdjson_language language)
{
  switch (language)
  {
  case PDJSON_LANGUAGE_JSON:
    json->flags &= ~(FLAG_JSON5 | FLAG_JSON5E);
    break;
  case PDJSON_LANGUAGE_JSON5:
    json->flags &= ~FLAG_JSON5E;
    json->flags |= FLAG_JSON5;
    break;
  case PDJSON_LANGUAGE_JSON5E:
    json->flags |= FLAG_JSON5 | FLAG_JSON5E;
    break;
  }
}

void
pdjson_close (pdjson_stream *json)
{
  if (json->alloc.malloc == NULL)
  {
    free (json->stack);
    free (json->data.string);
  }
  else
  {
    json->alloc.free (json->stack,
                      json->stack_size * sizeof (struct pdjson_stack),
                      json->alloc_data);
    json->alloc.free (json->data.string,
                      json->data.string_size,
                      json->alloc_data);
  }
}
