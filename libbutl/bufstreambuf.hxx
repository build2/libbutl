// file      : libbutl/bufstreambuf.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <cstdint>   // uint64_t
#include <streambuf>

#include <libbutl/export.hxx>

namespace butl
{
  // A buffered streambuf interface that exposes its buffer for direct scan
  // and provides a notion of logical position. See fdstreambuf for background
  // and motivation.
  //
  class LIBBUTL_SYMEXPORT bufstreambuf: public std::basic_streambuf<char>
  {
  public:
    using base = std::basic_streambuf<char>;

    using int_type = base::int_type;
    using traits_type = base::traits_type;

    using pos_type = base::pos_type; // std::streampos
    using off_type = base::off_type; // std::streamoff

  public:
    explicit
    bufstreambuf (std::uint64_t pos = 0): off_ (pos) {}

    virtual
    ~bufstreambuf ();

    // basic_streambuf input interface.
    //
  public:

    // Direct access to the get area. Use with caution.
    //
    using base::gptr;
    using base::egptr;
    using base::gbump;

    // Return the (logical) position of the next byte to be read.
    //
    // Note that on Windows when reading in the text mode the logical position
    // may differ from the physical file descriptor position due to the CRLF
    // character sequence translation. See the fdstreambuf::seekoff()
    // implementation for more background on this issue.
    //
    std::uint64_t
    tellg () const {return off_ - (egptr () - gptr ());}

    // basic_streambuf output interface.
    //
  public:

    // Return the (logical) position of the next byte to be written.
    //
    std::uint64_t
    tellp () const {return off_ + (pptr () - pbase ());}

  protected:
    std::uint64_t off_;
  };
}
