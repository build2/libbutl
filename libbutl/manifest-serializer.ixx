// file      : libbutl/manifest-serializer.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline void manifest_serializer::
  next (const std::string& n, const std::string& v)
  {
    if (!filter_ || filter_ (n, v))
      write_next (n, v);
  }
}
