// file      : libbutl/ft/exception.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_FT_EXCEPTION_HXX
#define LIBBUTL_FT_EXCEPTION_HXX

#if defined(__clang__)
#  if __has_include(<__config>)
#    include <__config>          // _LIBCPP_VERSION
#  endif
#endif

// __cpp_lib_uncaught_exceptions
//
#ifndef __cpp_lib_uncaught_exceptions
   //
   // VC has it since 1900.
   //
#  if defined(_MSC_VER)
#    if _MSC_VER >= 1900
#      define __cpp_lib_uncaught_exceptions 201411
#    endif
   //
   // Clang's libc++ seems to have it for a while (but not before 1200) so we
   // assume it's there from 1200. But not for MacOS, where it is explicitly
   // marked as unavailable until MacOS 10.12.
   //
#  elif defined(_LIBCPP_VERSION)
#    if _LIBCPP_VERSION >= 1200 &&                                      \
  (!defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) ||           \
   __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 101200)
#      define __cpp_lib_uncaught_exceptions 201411
#    endif
   //
   // GCC libstdc++ has it since GCC 6 and it defines the feature test macro.
   // We will also use this for any other runtime.
   //
#  endif
#endif

#endif // LIBBUTL_FT_EXCEPTION_HXX
