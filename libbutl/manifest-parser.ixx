// file      : libbutl/manifest-parser.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline manifest_name_value manifest_parser::
  next ()
  {
    manifest_name_value r;
    do { parse_next (r); } while (filter_ && !filter_ (r));
    return r;
  }
}
