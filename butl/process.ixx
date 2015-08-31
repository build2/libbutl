// file      : butl/process.ixx -*- C++ -*-
// copyright : Copyright (c) 2014-2015 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

namespace butl
{
  inline process::
  process (char const* args[], int in, int out, int err)
      : process (nullptr, args, in, out, err) {}

  inline process::
  process (char const* args[], process& in, int out, int err)
      : process (nullptr, args, in, out, err) {}
}
