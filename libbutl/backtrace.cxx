// file      : libbutl/backtrace.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules_ts
#include <libbutl/backtrace.mxx>
#endif

// We only enable backtrace during bootstrap if we can do it without any
// complications of the build scripts/makefiles.
//
// With glibc linking with -rdynamic gives (non-static) function names.
// FreeBSD requires explicitly linking -lexecinfo.
//
#ifndef BUILD2_BOOTSTRAP
#  if defined(__linux__) || \
      defined(__APPLE__) || \
      defined(__FreeBSD__)
#    define LIBBUTL_BACKTRACE
#  endif
#else
#  if defined(__linux__) || \
      defined(__APPLE__)
#    define LIBBUTL_BACKTRACE
#  endif
#endif

#ifdef LIBBUTL_BACKTRACE
#  include <stdlib.h>   // free()
#  include <execinfo.h>
#endif

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <string>

#ifdef LIBBUTL_BACKTRACE
#  include <memory>  // unique_ptr
#  include <cstddef> // size_t
#endif

#include <exception>
#endif

// Other includes.

#ifdef __cpp_modules_ts
module butl.backtrace;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules_ts
import std.core;
#endif
#endif

#endif

using namespace std;

namespace butl
{
  string
  backtrace () noexcept
  try
  {
    string r;

#ifdef LIBBUTL_BACKTRACE

    // Note: backtrace() returns int on Linux and MacOS and size_t on FreeBSD.
    //
    void* buf[1024];
    auto n (::backtrace (buf, 1024));

    assert (n >= 0);

    char** fs (backtrace_symbols (buf, n)); // Note: returns NULL on error.

    if (fs != nullptr)
    {
      unique_ptr<char*, void (*)(char**)> deleter (
        fs, [] (char** s) {::free (s);});

      for (size_t i (0); i != static_cast<size_t> (n); ++i)
      {
        r += fs[i];
        r += '\n';
      }
    }

#endif

    return r;
  }
  catch (const std::exception&)
  {
    return string ();
  }
}
