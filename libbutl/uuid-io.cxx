// file      : libbutl/uuid-io.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <libbutl/uuid-io.hxx>

#include <ostream>
#include <istream>
#include <stdexcept> // invalid_argument

using namespace std;

namespace butl
{
  ostream&
  operator<< (ostream& os, const uuid& u)
  {
    return os << u.c_string ().data ();
  }

  istream&
  operator>> (istream& is, uuid& u)
  {
    u = uuid ();

    char s[37];
    if (is.read (s, 36))
    {
      s[36] ='\0';

      try
      {
        u = uuid (s);
      }
      catch (const invalid_argument&)
      {
        is.setstate (istream::failbit);
      }
    }

    return is;
  }
}
