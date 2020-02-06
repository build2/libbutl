// file      : libbutl/ft/lang.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_FT_LANG_HXX
#define LIBBUTL_FT_LANG_HXX

// __cpp_thread_local (extension)
//
// If this macro is undefined then one may choose to fallback to __thread.
// Note, however, that it only for values that do not require dynamic
// (runtime) initialization.
//
#ifndef __cpp_thread_local
   //
   // Apparently Apple's Clang "temporarily disabled" C++11 thread_local until
   // they can implement a "fast" version, which reportedly happened in XCode
   // 8.
   //
#  if defined(__apple_build_version__)
#    if __apple_build_version__ >= 8000000
#      define __cpp_thread_local 201103
#    endif
#  else
#    define __cpp_thread_local 201103
#  endif
#endif

#endif // LIBBUTL_FT_LANG_HXX
