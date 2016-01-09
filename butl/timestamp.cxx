// file      : butl/timestamp.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/timestamp>

#include <time.h>  // localtime_r(), gmtime_r()
#include <errno.h> // EINVAL

#include <ctime>        // tm, strftime()
#include <iomanip>      // put_time(), setw(), dec, right
#include <cstring>      // strlen(), memcpy()
#include <ostream>
#include <stdexcept>    // runtime_error
#include <system_error>

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

using namespace details;
#endif

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
    }

    time_t t (system_clock::to_time_t (ts));

    std::tm tm;
    if ((local ? localtime_r (&t, &tm) : gmtime_r (&t, &tm)) == nullptr)
      throw system_error (errno, system_category ());

    using namespace chrono;

    timestamp sec (system_clock::from_time_t (t));
    nanoseconds ns (duration_cast<nanoseconds> (ts - sec));

    char fmt[256];
    size_t n (strlen (format));
    if (n + 1 > sizeof (fmt))
      throw system_error (EINVAL, system_category ());
    memcpy (fmt, format, n + 1);

    // Chunk the format string into fragments that we feed to put_time() and
    // those that we handle ourselves. Watch out for the escapes (%%).
    //
    size_t i (0), j (0); // put_time()'s range.
    for (; j != n; ++j)
    {
      if (fmt[j] == '%' && j + 1 != n)
      {
        if (fmt[j + 1] == '[')
        {
          if (os.width () != 0)
            throw runtime_error (
              "padding is not supported when printing nanoseconds");

          // Our fragment. First see if we need to call put_time().
          //
          if (i != j)
          {
            fmt[j] = '\0';
            if (!(os << put_time (&tm, fmt + i)))
              return os;
          }

          j += 2; // Character after '['.
          if (j == n)
            throw system_error (EINVAL, system_category ());

          char d ('\0');
          if (fmt[j] != 'N')
          {
            d = fmt[j];
            if (++j == n || fmt[j] != 'N')
              throw system_error (EINVAL, system_category ());
          }

          if (++j == n || fmt[j] != ']')
            throw system_error (EINVAL, system_category ());

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
        else
          ++j; // Skip % and the next character to handle %%.
      }
    }

    // Do we need to call put_time() one last time?
    //
    if (i != j)
    {
      if (!(os << put_time (&tm, fmt + i)))
        return os;
    }

    return os;
  }

  ostream&
  operator<< (ostream& os, const duration& d)
  {
    if (os.width () != 0) // We always print nanosecond.
      throw runtime_error (
        "padding is not supported when printing nanoseconds");

    timestamp ts; // Epoch.
    ts += d;

    time_t t (system_clock::to_time_t (ts));

    const char* fmt (nullptr);
    const char* unt ("nanoseconds");
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

    if (fmt != nullptr)
    {
      std::tm tm;
      if (gmtime_r (&t, &tm) == nullptr)
        throw system_error (errno, system_category ());

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

    timestamp sec (system_clock::from_time_t (t));
    nanoseconds ns (duration_cast<nanoseconds> (ts - sec));

    if (ns != nanoseconds::zero ())
    {
      if (fmt != nullptr)
      {
        ostream::fmtflags fl (os.flags ());
        char fc (os.fill ('0'));
        os << '.' << dec << right << setw (9) << ns.count ();
        os.fill (fc);
        os.flags (fl);
      }
      else
        os << ns.count ();

      os << ' ' << unt;
    }
    else if (fmt == nullptr)
      os << '0';

    return os;
  }
}
