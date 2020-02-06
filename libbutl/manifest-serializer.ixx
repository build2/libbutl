// file      : libbutl/manifest-serializer.ixx -*- C++ -*-
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
