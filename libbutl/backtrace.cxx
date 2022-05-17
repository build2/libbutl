// file      : libbutl/backtrace.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/backtrace.hxx>

// We only enable backtrace during bootstrap if we can do it without any
// complications of the build scripts/makefiles.
//
// With glibc linking with -rdynamic gives (non-static) function names.
// FreeBSD/NetBSD requires explicitly linking -lexecinfo. OpenBSD only has
// this functionality built-in from 7.0 and requires -lexecinfo.
//
// Note that some libc implementation on Linux (most notably, musl), don't
// support this, at least not out of the box.
//
#ifndef BUILD2_BOOTSTRAP
#  if defined(__GLIBC__)   || \
      defined(__APPLE__)   || \
      defined(__FreeBSD__) || \
      defined(__NetBSD__)
#    define LIBBUTL_BACKTRACE
#  elif defined (__OpenBSD__)
#    include <sys/param.h> // OpenBSD (yyyymm)
#    if OpenBSD >= 202110  // 7.0 was released in October 2021.
#      define LIBBUTL_BACKTRACE
#    endif
#  endif
#else
#  if defined(__GLIBC__) || \
      defined(__APPLE__)
#    define LIBBUTL_BACKTRACE
#  endif
#endif

#ifdef LIBBUTL_BACKTRACE
#  include <stdlib.h>   // free()
#  include <execinfo.h>
#endif

#include <cassert>

#ifdef LIBBUTL_BACKTRACE
#  include <memory>  // unique_ptr
#  include <cstddef> // size_t
#endif

#include <exception>

using namespace std;

namespace butl
{
  string
  backtrace () noexcept
  try
  {
    string r;

#ifdef LIBBUTL_BACKTRACE

    // Note: backtrace() returns int on Linux and MacOS and size_t on *BSD.
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
