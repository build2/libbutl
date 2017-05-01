// file      : libbutl/path-io.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_PATH_IO_HXX
#define LIBBUTL_PATH_IO_HXX

#include <ostream>

#include <libbutl/path.hxx>

namespace butl
{
  // This is the default path IO implementation. The reason it is
  // separate is because one often wants a custom implementation.
  // For example, we may want to print paths as relative to the
  // working directory. Or we may want to print '~' for the home
  // directory prefix. Or we may want to print dir_path with a
  // trailing '/'.
  //
  template <typename C, typename K>
  inline std::basic_ostream<C>&
  operator<< (std::basic_ostream<C>& os, basic_path<C, K> const& p)
  {
    return os << p.string ();
  }
}

#endif // LIBBUTL_PATH_IO_HXX
