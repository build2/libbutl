// file      : libbutl/path-io.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <cassert>
#include <ostream>

#include <libbutl/path.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // This is the default path IO implementation. It is separate to allow
  // custom implementations. For example, we may want to print paths as
  // relative to the working directory. Or we may want to print '~' for the
  // home directory prefix. Or we may want to print dir_path with a trailing
  // '/'.
  //
  template <typename C, typename K>
  inline std::basic_ostream<C>&
  operator<< (std::basic_ostream<C>& os, const basic_path<C, K>& p)
  {
    return to_stream (os, p, false /* representation */);
  }

  template <typename C, typename P>
  inline std::basic_ostream<C>&
  operator<< (std::basic_ostream<C>& os, const basic_path_name_view<P>& v)
  {
    assert (!v.null ());

    return v.name != nullptr && *v.name ? (os << **v.name) : (os << *v.path);
  }
}
