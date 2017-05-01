// file      : libbutl/process-io.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_PROCESS_IO_HXX
#define LIBBUTL_PROCESS_IO_HXX

#include <ostream>

#include <libbutl/process.hxx>

namespace butl
{
  inline std::ostream&
  operator<< (std::ostream& o, const process_path& p)
  {
    return o << p.recall_string ();
  }

  inline std::ostream&
  operator<< (std::ostream& o, const process_args& a)
  {
    process::print (o, a.argv, a.argc);
    return o;
  }
}

#endif // LIBBUTL_PROCESS_IO_HXX
