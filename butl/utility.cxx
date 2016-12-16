// file      : butl/utility.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/utility>

namespace butl
{
#ifndef BUTL_CXX17_UNCAUGHT_EXCEPTIONS

#ifdef BUTL_CXX11_THREAD_LOCAL
  thread_local
#else
  __thread
#endif
  bool exception_unwinding_dtor_ = false;

#ifdef _WIN32
  bool&
  exception_unwinding_dtor () {return exception_unwinding_dtor_;}
#endif

#endif
}
