// file      : butl/timestamp.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2015 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/timestamp>

#include <time.h> // localtime, gmtime, strftime

#include <ostream>
#include <system_error>

using namespace std;

namespace butl
{
  ostream&
  operator<< (ostream& os, const timestamp& ts)
  {
    // @@ replace with put_time()
    //

    time_t t (system_clock::to_time_t (ts));

    if (t == 0)
      return os << "<nonexistent>";

    std::tm tm;
    if (localtime_r (&t, &tm) == nullptr)
      throw system_error (errno, system_category ());

    // If year is greater than 9999, we will overflow.
    //
    char buf[20]; // YYYY-MM-DD HH:MM:SS\0
    if (strftime (buf, sizeof (buf), "%Y-%m-%d %H:%M:%S", &tm) == 0)
      return os << "<beyond year 9999>";

    os << buf;

    using namespace chrono;

    timestamp sec (system_clock::from_time_t (t));
    nanoseconds ns (duration_cast<nanoseconds> (ts - sec));

    if (ns != nanoseconds::zero ())
    {
      os << '.';
      os.width (9);
      os.fill ('0');
      os << ns.count ();
    }

    return os;
  }

  ostream&
  operator<< (ostream& os, const duration& d)
  {
    // @@ replace with put_time()
    //

    timestamp ts; // Epoch.
    ts += d;

    time_t t (system_clock::to_time_t (ts));

    const char* fmt (nullptr);
    const char* unt ("nanoseconds");
    if (t >= 365 * 12 * 24 * 60 * 60)
    {
      fmt = "%Y-%m-%d %H:%M:%S";
      unt = "years";
    }
    else if (t >= 12 * 24 * 60* 60)
    {
      fmt = "%m-%d %H:%M:%S";
      unt = "months";
    }
    else if (t >= 24 * 60* 60)
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

      char buf[20]; // YYYY-MM-DD HH:MM:SS\0
      if (strftime (buf, sizeof (buf), fmt, &tm) == 0)
        return os << "<beyond 9999>";

      os << buf;
    }

    using namespace chrono;

    timestamp sec (system_clock::from_time_t (t));
    nanoseconds ns (duration_cast<nanoseconds> (ts - sec));

    if (ns != nanoseconds::zero ())
    {
      if (fmt != nullptr)
      {
        os << '.';
        os.width (9);
        os.fill ('0');
      }

      os << ns.count () << ' ' << unt;
    }
    else if (fmt == 0)
      os << '0';

    return os;
  }
}
