// file      : libbutl/timestamp.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_TIMESTAMP_HXX
#define LIBBUTL_TIMESTAMP_HXX

#include <chrono>
#include <iosfwd>

#include <libbutl/export.hxx>

#include <libbutl/path.hxx>

namespace butl
{
  // On all three main platforms that we target (GNU/Linux, Windows (both
  // VC++ and GCC/MinGW64), and MacOS X) with recent C++ runtimes,
  // system_clock has nanoseconds resolution and counts from the UNIX
  // epoch. The latter is important since struct stat also returns times
  // based on UNIX epoch.
  //
  // The underlying type for nanoseconds duration is signed integer type
  // of at least 64 bits (currently int64_t, available as duration::rep).
  // Because it is signed, we will overflow in year 2262 but by then the
  // underlying type will most likely have changed to something larger
  // than 64-bit.
  //
  // So to support other platforms that could possibly use a different
  // system_clock resolutions (e.g., microseconds), we actually not going
  // to assume anywhere (except perhaps timestamp.cxx) that we are dealing
  // with nanoseconds or the 64-bit underlying type.
  //
  using std::chrono::system_clock;

  // Note that the default-initialized timestamp has the timestamp_nonexistent
  // value.
  //
  using timestamp = system_clock::time_point;
  using duration = system_clock::duration;

  // Generally-useful special values. Note that unknown is less than
  // nonexistent and is less than any non-special value.
  //
  const timestamp::rep timestamp_unknown_rep     = -1;
  const timestamp      timestamp_unknown         = timestamp (duration (-1));
  const timestamp::rep timestamp_nonexistent_rep = 0;
  const timestamp      timestamp_nonexistent     = timestamp (duration (0));

  // Print human-readable representation of the timestamp.
  //
  // By default the timestamp is printed by localtime_r() in the local
  // timezone, so tzset() from <time.h> should be called prior to using the
  // corresponding operator or the to_stream() function (normally from main()
  // or equivalent).
  //
  // The format argument in the to_stream() function is the put_time() format
  // string except that it also supports the nanoseconds conversion specifier
  // in the form %[<d>N] where <d> is the optional single delimiter character,
  // for example '.'. If the nanoseconds part is 0, then it is not printed
  // (nor the delimiter character). Otherwise, if necessary, the nanoseconds
  // part is padded to 9 characters with leading zeros.
  //
  // The special argument in the to_stream() function indicates whether the
  // special timestamp_unknown and timestamp_nonexistent values should be
  // printed as '<unknown>' and '<nonexistent>', respectively.
  //
  // The local argument in the to_stream() function indicates whether to use
  // localtime_r() or gmtime_r().
  //
  // Note also that these operators/function may throw std::system_error.
  //
  // Finally, padding is not fully supported by these operators/function. They
  // throw runtime_error if nanoseconds conversion specifier is present and
  // the stream's width field has been set to non-zero value before the call.
  //
  // Potential improvements:
  //  - add flag to to_stream() to use
  //  - support %[<d>U] (microseconds) and %[<d>M] (milliseconds).
  //  - make to_stream() a manipulator, similar to put_time()
  //  - support %(N) version for non-optional printing
  //  - support for suffix %[<d>N<s>], for example %[N nsec]
  //
  LIBBUTL_EXPORT std::ostream&
  to_stream (std::ostream&,
             const timestamp&,
             const char* format,
             bool special,
             bool local);

  // Same as above, but provide the result as string. Note that it is
  // implemented via to_stream() and std::ostringstream.
  //
  LIBBUTL_EXPORT std::string
  to_string (const timestamp&,
             const char* format,
             bool special,
             bool local);

  inline std::ostream&
  operator<< (std::ostream& os, const timestamp& ts)
  {
    return to_stream (os, ts, "%Y-%m-%d %H:%M:%S%[.N]", true, true);
  }

  LIBBUTL_EXPORT std::ostream&
  operator<< (std::ostream&, const duration&);

  // Parse human-readable representation of the timestamp.
  //
  // The format argument is the strptime() format string except that it also
  // supports the fraction of a second specifier in the form %[<d><f>], where
  // <d> is the optional single delimiter character, for example '.', and <f>
  // is one of the 'N', 'U', 'M' characters, denoting nanoseconds,
  // microseconds and milliseconds, respectively.
  //
  // The delimiter <d> is mandatory. If no such character is encountered at
  // the corresponding position of the input string, the function behaves as
  // if no %[] specifier were provided. Only single %[] specifier in the
  // format string is currently supported.
  //
  // If the delimiter is present, then it should be followed by 9 (N), 6 (U),
  // or 3 (M) digit value padded with leading zeros if necessary.
  //
  // If the local argument is true, then the input is assume to be local time
  // and the result is returned as local time as well. Otherwise, UCT is used
  // in both cases.
  //
  // If the end argument is not NULL, then it points to the first character
  // that was not parsed. Otherwise, throw invalid_argument in case of any
  // unparsed characters.
  //
  // Throw std::system_error on input/format mismatch and underlying time
  // conversion function failures.
  //
  // Note that internally from_string() calls strptime(), which behaves
  // according to the process' C locale (set with std::setlocale()) and not
  // the C++ locale (set with std::locale::global()). However the behaviour
  // can be affected by std::locale::global() as well, as it itself calls
  // std::setlocale() for the locale with a name.
  //
  // Potential improvements:
  //   - support %() version for non-optional component but with optional
  //     delimiter
  //   - ability to parse local, return UTC and vice-versa
  //   - handle timezone parsing
  //
  LIBBUTL_EXPORT timestamp
  from_string (const char* input,
               const char* format,
               bool local,
               const char** end = nullptr);
}

#endif // LIBBUTL_TIMESTAMP_HXX
