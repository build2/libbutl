#ifndef LIBPDJSON5_PDJSON5_H
#define LIBPDJSON5_PDJSON5_H

#ifndef LIBPDJSON5_SYMEXPORT
#  define LIBPDJSON5_SYMEXPORT
#endif

#ifdef __cplusplus
extern "C"
{
#else
#  if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#    include <stdbool.h>
#  else
#    ifndef bool
#      define bool int
#      define true 1
#      define false 0
#    endif // bool
#  endif // __STDC_VERSION__
#endif // __cplusplus

#include <stdio.h>
#include <stdint.h>
#include <stddef.h> // size_t

// Parsing event type.
//
enum pdjson_type
{
  PDJSON_ERROR = 1,
  PDJSON_DONE,
  PDJSON_OBJECT,
  PDJSON_OBJECT_END,
  PDJSON_ARRAY,
  PDJSON_ARRAY_END,
  PDJSON_NAME,       // Object member name.
  PDJSON_STRING,
  PDJSON_NUMBER,
  PDJSON_TRUE,
  PDJSON_FALSE,
  PDJSON_NULL
};

// Parsing event subtypes for the PDJSON_ERROR event.
//
enum pdjson_error_subtype
{
  PDJSON_ERROR_SYNTAX = 1,
  PDJSON_ERROR_MEMORY,
  PDJSON_ERROR_IO
};

// Custom allocator and io functions may throw provided the parser was
// compiled with exceptions support (-fexceptions, etc). In this case, the
// only valid operation on pdjson_stream is to close it with pdjson_close().
//
struct pdjson_allocator
{
  void *(*malloc) (size_t, void *user_data);
  void *(*realloc) (void *, size_t, void *user_data);
  void (*free) (void *, size_t, void *user_data);
};

// The peek() and get() functions are expected to return OEF on error, which
// can then be queried by calling error() (so essentially the stdio model).
// If the error() function is NULL, then assume there can be no io error.
//
// Note that we reasonably assume that if peek() did not fail, then the
// subsequent get() won't either. Likewise, if peek() did fail, then we assume
// the subsequent get() will return an error as well. Finally, we assume we
// can call failed peek() again with consistent results.
//
struct pdjson_user_io
{
  int (*peek) (void *user_data);
  int (*get) (void *user_data);
  bool (*error) (void *user_data);
};

typedef struct pdjson_stream pdjson_stream;
typedef struct pdjson_allocator pdjson_allocator;
typedef struct pdjson_user_io pdjson_user_io;

LIBPDJSON5_SYMEXPORT void
pdjson_open_buffer (pdjson_stream *json, const void *buffer, size_t size);

LIBPDJSON5_SYMEXPORT void
pdjson_open_string (pdjson_stream *json, const char *string);

LIBPDJSON5_SYMEXPORT void
pdjson_open_stream (pdjson_stream *json, FILE *stream);

LIBPDJSON5_SYMEXPORT void
pdjson_open_user (pdjson_stream *json,
                  const pdjson_user_io *user_io,
                  void *user_data);

// Open the parser without input. An attempt to parse in this state results in
// an input error. This ability is primarily useful to regularize reopening.
//
LIBPDJSON5_SYMEXPORT void
pdjson_open_null (pdjson_stream *json);

LIBPDJSON5_SYMEXPORT void
pdjson_close (pdjson_stream *json);

// Reopen the parser reusing any already allocated memory. Note that these
// functions are used instead of pdjson_close(), not in addition to it. All
// the other settings (allocator, features enabled, etc) are preserved. To
// regularize reopening, use pdjson_open_null().
//
LIBPDJSON5_SYMEXPORT void
pdjson_reopen_buffer (pdjson_stream *json, const void *buffer, size_t size);

LIBPDJSON5_SYMEXPORT void
pdjson_reopen_string (pdjson_stream *json, const char *string);

LIBPDJSON5_SYMEXPORT void
pdjson_reopen_stream (pdjson_stream *json, FILE *stream);

LIBPDJSON5_SYMEXPORT void
pdjson_reopen_user (pdjson_stream *json,
                    const pdjson_user_io *user_io,
                    void *user_data);

LIBPDJSON5_SYMEXPORT void
pdjson_reopen_null (pdjson_stream *json);


// Set custom allocator. Note that this should be done before performing any
// parsing.
//
LIBPDJSON5_SYMEXPORT void
pdjson_set_allocator (pdjson_stream *json,
                      const pdjson_allocator *allocator,
                      void *user_data);

LIBPDJSON5_SYMEXPORT void
pdjson_set_streaming (pdjson_stream *json, bool mode);

enum pdjson_language
{
  PDJSON_LANGUAGE_JSON,   // Strict JSON.
  PDJSON_LANGUAGE_JSON5,  // Strict JSON5
  PDJSON_LANGUAGE_JSON5E, // Extended JSON5.
};

LIBPDJSON5_SYMEXPORT void
pdjson_set_language (pdjson_stream *json, enum pdjson_language language);

LIBPDJSON5_SYMEXPORT enum pdjson_type
pdjson_next (pdjson_stream *json);

// Note that after peeking at the next even, all the accessor functions
// (pdjson_get_*_subtype(), pdjson_get_name/value(), pdjson_get_line/column(),
// pdjson_get_error(), etc), return information about the newly peeked event,
// not the previously consumed one.
//
LIBPDJSON5_SYMEXPORT enum pdjson_type
pdjson_peek (pdjson_stream *json);

LIBPDJSON5_SYMEXPORT void
pdjson_reset (pdjson_stream *json);

// Get subtype for certain events.
//
LIBPDJSON5_SYMEXPORT enum pdjson_error_subtype
pdjson_get_error_subtype (const pdjson_stream *json);

// Return the object member name after PDJSON_NAME event.
//
// Note that the returned size counts the trailing `\0`.
//
LIBPDJSON5_SYMEXPORT const char *
pdjson_get_name (const pdjson_stream *json, size_t *size);

// Return the string or number value after the PDJSON_STRING or PDJSON_NUMBER
// events.
//
// Note that the returned size counts the trailing `\0`.
//
LIBPDJSON5_SYMEXPORT const char *
pdjson_get_value (const pdjson_stream *json, size_t *size);

// Skip over the next value, skipping over entire arrays and objects. Return
// the skipped value.
//
LIBPDJSON5_SYMEXPORT enum pdjson_type
pdjson_skip (pdjson_stream *json);

// Skip until the specified event type or encountering PDJSON_ERROR or
// PDJSON_DONE. Return the encountered event.
//
LIBPDJSON5_SYMEXPORT enum pdjson_type
pdjson_skip_until (pdjson_stream *json, enum pdjson_type type);

LIBPDJSON5_SYMEXPORT uint64_t
pdjson_get_line (const pdjson_stream *json);

LIBPDJSON5_SYMEXPORT uint64_t
pdjson_get_column (const pdjson_stream *json);

LIBPDJSON5_SYMEXPORT uint64_t
pdjson_get_position (const pdjson_stream *json);

LIBPDJSON5_SYMEXPORT size_t
pdjson_get_depth (const pdjson_stream *json);

// Return the current parsing context, that is, PDJSON_OBJECT if we are inside
// an object, PDJSON_ARRAY if we are inside an array, and PDJSON_DONE if we
// are not yet/no longer in either, or PDJSON_ERROR if the parser is in the
// error state.
//
// Additionally, for the first two cases, return the number of parsing events
// that have already been observed at this level with pdjson_next/peek(). In
// particular, inside an object, an odd number would indicate that we just
// observed the PDJSON_NAME event.
//
LIBPDJSON5_SYMEXPORT enum pdjson_type
pdjson_get_context (const pdjson_stream *json, uint64_t *count);

// Return error message if the previously peeked at or consumed even was
// PDJSON_ERROR and NULL otherwise. Note that the message is UTF-8 encoded.
//
LIBPDJSON5_SYMEXPORT const char *
pdjson_get_error (const pdjson_stream *json);

// Direct byte stream access.
//
LIBPDJSON5_SYMEXPORT int
pdjson_source_get (pdjson_stream *json);

LIBPDJSON5_SYMEXPORT int
pdjson_source_peek (pdjson_stream *json);

LIBPDJSON5_SYMEXPORT bool
pdjson_source_error (pdjson_stream *json);

// Note that this function only examines the first byte of a potentially
// multi-byte UTF-8 sequence. As result, it only returns true for whitespaces
// encoded single bytes. Those are the only valid ones for JSON but not for
// JSON5. If you need to detect multi-byte whitespaces, then you will either
// need to do this yourself (and diagnose any non-whitespaces as appropriate)
// or use pdjson_skip_if_space() below.
//
LIBPDJSON5_SYMEXPORT bool
pdjson_is_space (const pdjson_stream *json, int byte);

// Given a peeked at byte, consume it and any following bytes that are part of
// the same multi-byte UTF-8 sequence if it is a whitespace and return 1. If
// it is a part of a multi-byte UTF-8 sequence but is not a whitespace,
// consume it, trigger an error, and return -1 (a codepoint that requires
// multiple bytes is only valid in JSON strings). Otherwise (single-byte
// non-whitespace), don't consume it and return 0.
//
// If the result is 1 and codepoint is not NULL, then set it to the decoded
// whitespace codepoint.
//
// Note that in the JSON5/JSON5E mode this function also skips comments,
// treating each as a single logical whitespace (but you can omit skipping
// comments by pre-checking the peeked byte for '/' and '#'). In this case,
// codepoint will contain the comment determinant character (`/`, `*`, `#`).
// Note that for the line comments (`//' and `#`), the newline is part of the
// comment.
//
// This function is primarily meant for custom handling of separators between
// values in the streaming mode. Its semantics is rather convoluted due to the
// source get/peek interface operating on bytes, not codepoints.
//
LIBPDJSON5_SYMEXPORT int
pdjson_skip_if_space (pdjson_stream *json, int byte, uint32_t *codepoint);

// Implementation details.
//

enum pdjson_source_tag
{
  PDJSON_SOURCE_BUFFER = 1,
  PDJSON_SOURCE_USER,
  PDJSON_SOURCE_STREAM,
  PDJSON_SOURCE_NULL
};

struct pdjson_source
{
  enum pdjson_source_tag tag;
  uint64_t position;
  union
  {
    struct
    {
      FILE *stream;
    } stream;

    struct
    {
      const char *buffer;
      size_t length;
    } buffer;

    struct
    {
      void *data;
      pdjson_user_io io;
    } user;
  } source;
};

struct pdjson_stream
{
  uint64_t lineno;

  // While counting lines is straightforward, columns are tricky because we
  // have to count codepoints, not bytes. We could have peppered the code with
  // increments in all the relevant places but that seems inelegant. So
  // instead we calculate the column dynamically, based on the current
  // position.
  //
  // Specifically, we will remember the position at the beginning of each line
  // (linepos) and, assuming only the ASCII characters on the line, the column
  // will be the difference between the current position and linepos. Of
  // course there could also be multi-byte UTF-8 sequences which we will
  // handle by keeping an adjustment (lineadj) -- the number of continuation
  // bytes encountered on this line so far. Finally, for pdjson_source_get()
  // we also have to keep the number of remaining continuation bytes in the
  // current multi-byte UTF-8 sequence (linecon).
  //
  // This is not the end of the story, however: with only the just described
  // approach we will always end up with the column of the latest character
  // read which is not what we want when returning potentially multi-
  // character value events (string, number, etc); in these cases we want to
  // return the column of the first character (note that if the value itself
  // is invalid and we are returning PDJSON_ERROR, we still want the current
  // column). So to handle this we will cache the start column (colno) for
  // such events.
  //
  uint64_t linepos; // Position at the beginning of the current line.
  size_t   lineadj; // Adjustment for multi-byte UTF-8 sequences.
  size_t   linecon; // Number of remaining UTF-8 continuation bytes.

  // Start line/column for value events or 0.
  //
  uint64_t start_lineno;
  uint64_t start_colno;

  struct pdjson_stack *stack;
  size_t stack_top;
  size_t stack_size;
  uint32_t flags;

  unsigned int subtype; // One of enum pdjson_*_subtype.
  enum pdjson_type peek;

  struct
  {
    enum pdjson_type type;
    unsigned int subtype;
    uint64_t lineno;
    uint64_t colno;
  } pending;

  struct
  {
    char *string;
    size_t string_fill;
    size_t string_size;
  } data;

  uint64_t ntokens; // Number of values/names read, recursively.

  struct pdjson_source source;
  struct pdjson_allocator alloc;
  void *alloc_data;

  char error_message[128];
  char utf8_char[6]; // Up to 4 for UTF-8, 2 for quotes, one for \0.
};

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // LIBPDJSON5_PDJSON5_H
