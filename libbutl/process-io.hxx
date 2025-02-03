// file      : libbutl/process-io.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

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

  inline std::ostream&
  operator<< (std::ostream& o, const process_env& e)
  {
    return to_stream (o, e);
  }
}
