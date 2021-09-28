// file      : libbutl/timestamp.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/timestamp.hxx>

#include <time.h>  // localtime_{r,s}(), gmtime_{r,s}(), strptime(), timegm()
#include <errno.h> // EINVAL

#include <cassert> // Note: also gets __GLIBCXX__.

// Implementation of strptime() and timegm() for Windows.
//
// Here we have several cases. If this is VC++, then we implement strptime()
// via C++11 std::get_time(). And if this is MINGW GCC (or, more precisely,
// libstdc++), then we have several problems. Firstly, GCC prior to 5 doesn't
// implement std::get_time(). Secondly, GCC 5 and even 6 have buggy
// std::get_time() (it cannot parse single-digit days). So what we are going
// to do in this case is use a FreeBSD-based strptime() implementation.
//
// Include the C implementation here, out of module purview.
//
#ifdef _WIN32
#ifdef __GLIBCXX__
extern "C"
{
#  include "strptime.c"
}
#else
#  include <locale.h> // LC_ALL
#endif
#endif

#include <ctime>        // tm, time_t, mktime(), strftime()[libstdc++]
#include <cstdlib>      // strtoull()
#include <sstream>      // ostringstream, stringstream[VC]
#include <iomanip>      // put_time(), setw(), dec, right
#include <cstring>      // strlen(), memcpy(), strchr()[VC]
#include <ostream>
#include <utility>      // pair, make_pair()
#include <stdexcept>    // runtime_error

// Implementation of strptime() for VC.
//
#ifdef _WIN32
#ifndef __GLIBCXX__
#  include <ios>
#  include <locale>
#  include <clocale>
#  include <iomanip>
#endif
#endif

#include <libbutl/utility.hxx> // throw_generic_error()

using namespace std;

// libstdc++ prior to GCC 5 does not have std::put_time() so we have to invent
// our own. Detecting the "prior to GCC 5" condition, however, is not easy:
// libstdc++ is used by other compilers (e.g., Clang) so we cannot just use
// __GNUC__. There is __GLIBCXX__ but it is a date which is updated with
// every release, including bugfixes (so, there can be some 4.7.X release with
// a date greater than 5.0.0).
//
// So what we going to do here is "offer" our implementation and let the ADL
// pick one. If there is std::put_time(), then it will be preferred because
// of the std::tm argument.
//
#ifdef __GLIBCXX__
namespace butl
{
  namespace details
  {
    struct put_time_data
    {
      const std::tm* tm;
      const char* fmt;
    };

    inline put_time_data
    put_time (const std::tm* tm, const char* fmt)
    {
      return put_time_data {tm, fmt};
    }

    inline ostream&
    operator<< (ostream& os, const put_time_data& d)
    {
      char buf[256];
      if (strftime (buf, sizeof (buf), d.fmt, d.tm) != 0)
        os << buf;
      else
        os.setstate (ostream::badbit);
      return os;
    }
  }
}

using namespace butl::details;
#endif

// Thread-safe implementations of gmtime() and localtime().
//
// Normally we would provide POSIX function replacement for Windows if the
// original function is absent. However, MinGW GCC can sometimes provide them.
// And so to avoid name clashes we hide them in the details namespace.
//
// Previously we have used gmtime_s() and localtime_s() for gmtime() and
// localtime() implementations for Windows, but that required Security-Enhanced
// version of CRT to be present, which is not always the case. In particular if
// MinGW is configured with --disable-secure-api option then declarations of
// *_s() functions are not available. So we use ::gmtime() and ::localtime()
// for that purpose. Note that according to MSDN "gmtime and localtime all use
// one common tm structure per thread for the conversion", which mean that they
// are thread-safe.
//
namespace butl
{
  namespace details
  {
    static tm*
    gmtime (const time_t* t, tm* r)
    {
#ifdef _WIN32
      const tm* gt (::gmtime (t));
      if (gt == nullptr)
        return nullptr;

      *r = *gt;
      return r;
#else
      return gmtime_r (t, r);
#endif
    }

    static tm*
    localtime (const time_t* t, tm* r)
    {
#ifdef _WIN32
      const tm* lt (::localtime (t));
      if (lt == nullptr)
        return nullptr;

      *r = *lt;
      return r;
#else
      return localtime_r (t, r);
#endif
    }
  }
}

// Implementation of strptime() for VC and timegm() for Windows.
//
#ifdef _WIN32
#ifndef __GLIBCXX__
static char*
strptime (const char* input, const char* format, tm* time)
{
  // VC std::get_time()-based implementation.
  //
  // Note that the major difference in semantics of strptime() and
  // std::get_time() is that the former always fails if the format string is
  // not fully processed, while the latter can succeed in such a case,
  // specifically if the end of the stream is reached after a conversion
  // specifier was successfully applied. See this post for some background:
  //
  // https://stackoverflow.com/questions/67060956/what-is-the-correct-behavior-of-stdget-time-for-short-input
  //
  // The consequence of this fact is that there is no easy way to detect if
  // the format was fully processed when the end of input is reached. It seems
  // that the only way to resolve this ambiguity is to append some end marker
  // to both the input and format and re-parse. We can dedicate some character
  // that is unlikely to be used in the time format/input (for example '\x1')
  // to serve as an end marker.
  //
  // Alternatively, we can abandon the use of std::get_time() altogether and,
  // for example, use a FreeBSD-based strptime() implementation. This feels a
  // bit too radical at the moment, though.
  //
  const char em ('\x1');

  if (strchr (input, em) != nullptr || strchr (format, em) != nullptr)
    return nullptr;

  stringstream ss (input); // Input/output stream.

  // The original strptime() function behaves according to the process' C
  // locale (set with std::setlocale()), which can differ from the process C++
  // locale (set with std::locale::global()).
  //
  ss.imbue (locale (setlocale (LC_ALL, nullptr)));

  // Bail out on the parsing error.
  //
  if (!(ss >> get_time (time, format)))
    return nullptr;

  // If the end of input is not reached then the format string is fully
  // processed.
  //
  if (!ss.eof ())
    return const_cast<char*> (input + static_cast<size_t> (ss.tellg ()));

  // Since eof is reached, we cannot say if the format string was fully
  // processed or not. For example:
  //
  // %b %Y     - format
  // Feb 2016  - eofbit is set with a format fully processed
  // Feb       - eofbit is set with a format partially processed
  //
  // So append the end marker character to both input and format and re-parse.
  //
  ss.clear ();                 // Clear eof.
  ss.seekp (0, ios_base::end); // Position to the end for writing.
  ss.put (em);                 // Append the end marker.
  ss.seekg (0);                // Rewind for reading.

  string fm (format);
  fm += em;                    // Append the end marker.

  // Fail if the input is "shorter" than the format. For example:
  //
  // %b %Y\x1  - format
  // Feb\x1    - stream
  //
  // Note that we can reuse the time object for re-parsing, since on success
  // its fields will be overwritten with the same values.
  //
  if (!(ss >> get_time (time, fm.c_str ())))
    return nullptr;

  // We would fail earlier otherwise.
  //
  assert (ss.eof () || ss.get () == stringstream::traits_type::eof ());

  // tellg() behaves as UnformattedInputFunction, so returns failure status if
  // eofbit is set.
  //
  return const_cast<char*> (input + strlen (input));
}
#endif

static time_t
timegm (tm* ctm)
{
  const time_t e (static_cast<time_t> (-1));

  // We will use an example to explain how it works. Say *ctm contains 9 AM of
  // some day. Note that no time zone information is available.
  //
  // Convert it to the time from Epoch as if it's in the local time zone.
  //
  ctm->tm_isdst = -1;
  time_t t (mktime (ctm));
  if (t == e)
    return e;

  // Let's say we are in Moscow, and t contains the time passed from Epoch till
  // 9 AM MSK. But that is not what we need. What we need is the time passed
  // from Epoch till 9 AM GMT. This is some bigger number, as it takes longer
  // to achieve the same calendar time for more Western location. So we need to
  // find that offset, and increment t with it to obtain the desired value. The
  // offset is effectively the time difference between MSK and GMT time zones.
  //
  tm gtm;
  if (butl::details::gmtime (&t, &gtm) == nullptr)
    return e;

  // gmtime() being called for the timepoint t returns 6 AM. So now we have
  // *ctm and gtm, which value difference (3 hours) reflects the desired
  // offset. The only problem is that we can not deduct gtm from *ctm, to get
  // the offset expressed as time_t. To do that we need to apply to both of
  // them the same conversion function transforming std::tm to std::time_t. The
  // mktime() can do that, so the expression (mktime(ctm) - mktime(&gtm))
  // calculates the desired offset.
  //
  // To ensure mktime() works exactly the same way for both cases, we need to
  // reset Daylight Saving Time flag for each of *ctm and gtm.
  //
  ctm->tm_isdst = 0;
  time_t lt (mktime (ctm));
  if (lt == e)
    return e;

  gtm.tm_isdst = 0;
  time_t gt (mktime (&gtm));
  if (gt == e)
    return e;

  // C11 standard specifies time_t to be a real type (integer and real floating
  // types are collectively called real types). So we can not consider it to be
  // signed.
  //
  return lt > gt ? t + (lt - gt) : t - (gt - lt);
}
#endif // _WIN32

namespace butl
{
  ostream&
  to_stream (ostream& os,
             const timestamp& ts,
             const char* format,
             bool special,
             bool local)
  {
    if (special)
    {
      if (ts == timestamp_unknown)
        return os << "<unknown>";

      if (ts == timestamp_nonexistent)
        return os << "<nonexistent>";

      if (ts == timestamp_unreal)
        return os << "<unreal>";
    }

    time_t t (system_clock::to_time_t (ts));

    std::tm tm;
    if ((local
         ? details::localtime (&t, &tm)
         : details::gmtime (&t, &tm)) == nullptr)
      throw_generic_error (errno);

    using namespace chrono;

    timestamp sec (system_clock::from_time_t (t));
    nanoseconds ns (duration_cast<nanoseconds> (ts - sec));

    char fmt[256];
    size_t n (strlen (format));
    if (n + 1 > sizeof (fmt))
      throw_generic_error (EINVAL);
    memcpy (fmt, format, n + 1);

    // Chunk the format string into fragments that we feed to put_time() and
    // those that we handle ourselves. Watch out for the escapes (%%).
    //
    size_t i (0), j (0); // put_time()'s range.

    // Print the time partially, using the not printed part of the format
    // string up to the current scanning position and setting the current
    // character to NULL. Return true if the operation succeeded.
    //
    auto print = [&i, &j, &fmt, &os, &tm] ()
    {
      if (i != j)
      {
        fmt[j] = '\0'; // Note: there is no harm if j == n.
        if (!(os << put_time (&tm, fmt + i)))
          return false;
      }

      return true;
    };

    for (; j != n; ++j)
    {
      if (fmt[j] == '%' && j + 1 != n)
      {
        char c (fmt[j + 1]);

        if (c == '[')
        {
          if (os.width () != 0)
            throw runtime_error (
              "padding is not supported when printing nanoseconds");

          // Our fragment.
          //
          if (!print ())
            return os;

          j += 2; // Character after '['.
          if (j == n)
            throw_generic_error (EINVAL);

          char d ('\0');
          if (fmt[j] != 'N')
          {
            d = fmt[j];
            if (++j == n || fmt[j] != 'N')
              throw_generic_error (EINVAL);
          }

          if (++j == n || fmt[j] != ']')
            throw_generic_error (EINVAL);

          if (ns != nanoseconds::zero ())
          {
            if (d != '\0')
              os << d;

            ostream::fmtflags fl (os.flags ());
            char fc (os.fill ('0'));
            os << dec << right << setw (9) << ns.count ();
            os.fill (fc);
            os.flags (fl);
          }

          i = j + 1; // j is incremented in the for-loop header.
        }
        //
        // Note that MinGW (as of GCC 9.2) libstdc++'s implementations of
        // std::put_time() and std::strftime() don't recognize the %e
        // specifier, converting it into the empty string (bug #95624). Thus,
        // we handle this specifier ourselves for libstdc++ on Windows.
        //
#if defined(_WIN32) && defined(__GLIBCXX__)
        else if (c == 'e')
        {
          if (!print ())
            return os;

          ostream::fmtflags fl (os.flags ());
          char fc (os.fill (' '));

          // Note: the width is automatically reset to 0 (unspecified) after
          // we print the day.
          //
          os << dec << right << setw (2) << tm.tm_mday;

          os.fill (fc);
          os.flags (fl);

          ++j;       // Positions at 'e'.
          i = j + 1; // j is incremented in the for-loop header.
        }
#endif
        else
          ++j; // Skip % and the next character to handle %%.
      }
    }

    print (); // Call put_time() one last time, if required.
    return os;
  }

  string
  to_string (const timestamp& ts,
             const char* format,
             bool special,
             bool local)
  {
    ostringstream o;
    to_stream (o, ts, format, special, local);
    return o.str ();
  }

  ostream&
  to_stream (ostream& os, const duration& d, bool ns)
  {
    if (os.width () != 0) // We always print nanosecond.
      throw runtime_error (
        "padding is not supported when printing nanoseconds");

    timestamp ts; // Epoch.
    ts += d;

    time_t t (system_clock::to_time_t (ts));

    const char* fmt (nullptr);
    const char* unt;
    if (t >= 365 * 24 * 60 * 60)
    {
      fmt = "%Y-%m-%d %H:%M:%S";
      unt = "years";
    }
    else if (t >= 31 * 24 * 60 * 60)
    {
      fmt = "%m-%d %H:%M:%S";
      unt = "months";
    }
    else if (t >= 24 * 60 * 60)
    {
      fmt = "%d %H:%M:%S";
      unt = "days";
    }
    else if (t >= 60 * 60)
    {
      fmt = "%H:%M:%S";
      unt = "hours";
    }
    else if (t >= 60)
    {
      fmt = "%M:%S";
      unt = "minutes";
    }
    else if (t >= 1)
    {
      fmt = "%S";
      unt = "seconds";
    }
    else
      unt = ns ? "nanoseconds" : "seconds";

    if (fmt != nullptr)
    {
      std::tm tm;
      if (details::gmtime (&t, &tm) == nullptr)
        throw_generic_error (errno);

      if (t >= 24 * 60 * 60)
        tm.tm_mday -= 1; // Make day of the month to be a zero-based number.

      if (t >= 31 * 24 * 60 * 60)
        tm.tm_mon -= 1; // Make month of the year to be a zero-based number.

      if (t >= 365 * 24 * 60 * 60)
        // Make the year to be a 1970-based number. Negative values allowed
        // according to the POSIX specification.
        //
        tm.tm_year -= 1970;

      if (!(os << put_time (&tm, fmt)))
        return os;
    }

    using namespace chrono;

    if (ns)
    {
      timestamp sec (system_clock::from_time_t (t));
      nanoseconds nsec (duration_cast<nanoseconds> (ts - sec));

      if (nsec != nanoseconds::zero ())
      {
        if (fmt != nullptr)
        {
          ostream::fmtflags fl (os.flags ());
          char fc (os.fill ('0'));
          os << '.' << dec << right << setw (9) << nsec.count ();
          os.fill (fc);
          os.flags (fl);
        }
        else
          os << nsec.count ();
      }
      else if (fmt == nullptr)
        os << '0';
    }
    else if (fmt == nullptr)
      os << '0';

    os << ' ' << unt;
    return os;
  }

  string
  to_string (const duration& d, bool nsec)
  {
    ostringstream o;
    to_stream (o, d, nsec);
    return o.str ();
  }

  static pair<tm, chrono::nanoseconds>
  from_string (const char* input, const char* format, const char** end)
  {
    auto bad_val = [] () {throw_generic_error (EINVAL);};

    // See if we have our specifier.
    //
    size_t i (0);
    size_t n (strlen (format));
    for (; i != n; ++i)
    {
      if (format[i] == '%' && i + 1 != n)
      {
        if (format[i + 1] == '[')
          break;
        else
          ++i; // To handle %%.
      }
    }

    // Call the fraction of a second as just fraction from now on.
    //
    using namespace chrono;
    nanoseconds ns (nanoseconds::zero ());

    if (i == n)
    {
      // No %[], so just parse with strptime().
      //
      tm t = tm ();
      const char* p (strptime (input, format, &t));
      if (p == nullptr)
        bad_val ();

      if (end != nullptr)
        *end = p;
      else if (*p != '\0')
        bad_val (); // Input is not fully read.

      t.tm_isdst = -1;
      return make_pair (t, ns);
    }

    // Now the overall plan is:
    //
    // 1. Parse the fraction part of the input string to obtain nanoseconds.
    //
    // 2. Remove fraction part from the input string.
    //
    // 3. Remove %[] from the format string.
    //
    // 4. Re-parse the modified input with the modified format to fill the
    //    std::tm structure.
    //
    // Parse the %[] specifier.
    //
    assert (format[i] == '%');
    string fm (format, i++); // Start assembling the new format string.

    assert (format[i] == '[');
    if (++i == n)
      bad_val ();

    char d (format[i]); // Delimiter character.
    if (++i == n)
      bad_val ();

    char f (format[i]); // Fraction specifier character.
    if ((f != 'N' && f != 'U' && f != 'M') || ++i == n)
      bad_val ();

    if (format[i++] != ']')
      bad_val ();

    // Parse the input with the initial part of the format string, the one
    // that preceeds the %[] specifier. The returned pointer will be the
    // position we need to start from to parse the fraction.
    //
    tm t = tm ();

    // What if %[] is first, there is nothing before it? According to the
    // strptime() documentation an empty format string is a valid one.
    //
    const char* p (strptime (input, fm.c_str (), &t));
    if (p == nullptr)
      bad_val ();

    // Start assembling the new input string.
    //
    string in (input, p - input);
    size_t fn (0); // Fraction size.

    if (d == *p)
    {
      // Fraction present in the input.
      //

      // Read fraction digits.
      //
      char buf [10];
      size_t i (0);
      size_t n (f == 'N' ? 9 : (f == 'U' ? 6 : 3));
      for (++p; i < n && *p >= '0' && *p <= '9'; ++i, ++p)
        buf[i] = *p;

      if (i < n)
        bad_val ();

      buf[n] = '\0';
      fn = n;

      // Calculate nanoseconds.
      //
      char* e (nullptr);
      unsigned long long t (strtoull (buf, &e, 10));
      assert (e == buf + n);

      switch (f)
      {
      case 'N': ns = nanoseconds (t); break;
      case 'U': ns = microseconds (t); break;
      case 'M': ns = milliseconds (t); break;
      default: assert (false);
      }

      // Actually the idea to fully remove the fraction from the input string,
      // and %[] from the format string, has a flaw. After the fraction removal
      // the spaces around it will be "swallowed" with a single space in the
      // format string. So, as an example, for the input:
      //
      // 2016-02-21 19:31:10 .384902285 GMT
      //
      // And the format:
      //
      // %Y-%m-%d %H:%M:%S %[.N]
      //
      // The unparsed tail of the input will be 'GMT' while expected to be
      // ' GMT'. To fix that we will not remove, but replace the mentioned
      // parts with some non-space character.
      //
      fm += '-';
      in += '-';
    }

    fm += format + i;
    in += p;

    // Reparse the modified input with the modified format.
    //
    t = tm ();
    const char* b (in.c_str ());
    p = strptime (b, fm.c_str (), &t);

    if (p == nullptr)
      bad_val ();

    if (end != nullptr)
      *end = input + (p - b + fn);
    else if (*p != '\0')
      bad_val (); // Input is not fully read.

    t.tm_isdst = -1;
    return make_pair (t, ns);
  }

  timestamp
  from_string (const char* input,
               const char* format,
               bool local,
               const char** end)
  {
    pair<tm, chrono::nanoseconds> t (from_string (input, format, end));

    time_t time (local ? mktime (&t.first) : timegm (&t.first));
    if (time == -1)
      throw_generic_error (errno);

    return system_clock::from_time_t (time) +
      chrono::duration_cast<duration> (t.second);
  }

  duration
  daytime (timestamp t)
  {
    // Convert the time from Epoch to the local time.
    //
    time_t time (system_clock::to_time_t (t));

    std::tm tm;
    if (details::localtime (&time, &tm) == nullptr)
      throw_generic_error (errno);

    // Roll the time back to the latest midnight.
    //
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;

    // Convert the local midnight to time from Epoch.
    //
    time = mktime (&tm);
    if (time == -1)
      throw_generic_error (errno);

    // Note: preserves the accuracy of the original time.
    //
    return t - system_clock::from_time_t (time);
  }
}
