#pragma once

// Make sure _MSVC_STL_VERSION is defined for MSVC STL, __GLIBCXX__ for
// libstdc++, etc.
//
#include <cstddef>

#include <libstud/regex/export.hxx>

// Note that MSVC 12.0 (VS 2013) added the initial support for C++14 (see the
// "Microsoft C/C++ language conformance by Visual Studio version"
// documentation for details). However, until MSVC 15.7 (VS 2017) the
// __cplusplus value remained at 199711L (C++98 / C++03; see the "MSVC now
// correctly reports __cplusplus" article for details).
//
#ifdef __cplusplus
#  if __cplusplus <= 201103L
#    if defined(_MSC_VER) && _MSC_VER >= 1800 // MSVC >= 12.0
#      define LIBSTUD_REGEX_STD_VER 14
#    else
#      error C++14 compiler required
#    endif
#  elif __cplusplus <= 201402L
#    define LIBSTUD_REGEX_STD_VER 14
#  elif __cplusplus <= 201703L
#    define LIBSTUD_REGEX_STD_VER 17
#  elif __cplusplus <= 202002L
#    define LIBSTUD_REGEX_STD_VER 20
#  elif __cplusplus <= 202302L
#    define LIBSTUD_REGEX_STD_VER 23
#  elif __cplusplus <= 202603L
#    define LIBSTUD_REGEX_STD_VER 26
#  else
// Expected release year of the next C++ standard
#    define LIBSTUD_REGEX_STD_VER 29
#  endif
#else
#  error C++14 compiler required
#endif // __cplusplus

#undef LIBSTUD_REGEX_CXX03_LANG
#undef LIBSTUD_REGEX_USE_FROZEN_CXX03_HEADERS

// Note that some early implementations of C++20 standard libraries didn't
// provide the <version> header (libstdc++ < 9.0, MSVC STL < 14.22, etc; see
// the "Microsoft C/C++ language conformance by Visual Studio version"
// documentation for details). So let's always check the header presence for
// libstdc++ and MSVC runtimes for C++20.
//
// Also note that the __has_include() preprocessor operator was introduced in
// C++17.
//
// And yet, _MSVC_STL_VERSION was introduced in MSVC STL 14.12, while
// _CPPLIB_VER was introduced much earlier but doesn't change since
// _MSVC_STL_VERSION came into play.
//
#if LIBSTUD_REGEX_STD_VER >= 20
#  if LIBSTUD_REGEX_STD_VER > 20 ||   \
      (!defined(__GLIBCXX__)       && \
       !defined(_MSVC_STL_VERSION) && \
       !defined(_CPPLIB_VER))    ||   \
      __has_include(<version>)
#    include <version>
#  endif
#endif

// Avoid using `#pragma GCC system_header` which reduces noise, disabling
// warnings, etc.
//
#define LIBSTUD_REGEX_HAS_NO_PRAGMA_SYSTEM_HEADER    1

#define LIBSTUD_REGEX_BEGIN_NAMESPACE_STD            namespace stud { namespace regex {
#define LIBSTUD_REGEX_BEGIN_EXPLICIT_ABI_ANNOTATIONS
#define LIBSTUD_REGEX_END_EXPLICIT_ABI_ANNOTATIONS
#define LIBSTUD_REGEX_END_NAMESPACE_STD              } }

#define LIBSTUD_REGEX_PUSH_MACROS
#define LIBSTUD_REGEX_POP_MACROS

#define LIBSTUD_REGEX_HIDE_FROM_ABI
#define LIBSTUD_REGEX_HIDE_FROM_ABI_VIRTUAL
#define LIBSTUD_REGEX_EXPORTED_FROM_ABI              LIBSTUD_REGEX_SYMEXPORT

#define LIBSTUD_REGEX_CONSTEXPR                      constexpr
#define LIBSTUD_REGEX_CONSTEXPR_SINCE_CXX14          constexpr

#define LIBSTUD_REGEX_HAS_EXCEPTIONS                 1
#define LIBSTUD_REGEX_ABI_REGEX_CONSTANTS_NONZERO    1

#define LIBSTUD_REGEX_HAS_LOCALIZATION               1
#undef  LIBSTUD_REGEX_HAS_NO_LOCALIZATION

#define LIBSTUD_REGEX_HAS_WIDE_CHARACTERS            1
#undef  LIBSTUD_REGEX_HAS_NO_WIDE_CHARACTERS

#define LIBSTUD_REGEX_AVAILABILITY_PMR

#define LIBSTUD_REGEX_VERBOSE_ABORT(x)               std::abort ()
#define LIBSTUD_REGEX_PREFERRED_NAME(x)
#define LIBSTUD_REGEX_IF_WIDE_CHARACTERS(x)

#define LIBSTUD_REGEX_NODEBUG
#define LIBSTUD_REGEX_ASSERT_PEDANTIC(e, m)             assert (e)
#define LIBSTUD_REGEX_ASSERT_VALID_ELEMENT_ACCESS(e, m) assert (e)

#define LIBSTUD_REGEX_WARN_UNUSED

#define LIBSTUD_REGEX_NOEXCEPT       noexcept
#define LIBSTUD_REGEX_NOEXCEPT_(...) noexcept(__VA_ARGS__)

#if defined(__NEWLIB__) || defined(_NEWLIB_VERSION)
#  define LIBSTUD_REGEX_LIBC_NEWLIB                  1
#endif

// Note that some older compilers (GCC 4.9, etc) don't support the
// __has_cpp_attribute() preprocessor operator, introduced in C++11. We will
// assume that if we have no means for checking for the attributes support,
// the none of them is supported.
//
#ifdef __has_cpp_attribute
#  define LIBSTUD_REGEX_HAS_CPP_ATTRIBUTE(...) __has_cpp_attribute(__VA_ARGS__)
#else
#  define LIBSTUD_REGEX_HAS_CPP_ATTRIBUTE(...) 0
#endif

// Note that Clang 3.7 doesn't provide complete support for C++17. In
// particular, it doesn't support the nodiscard and fallthrough attributes.
//
#if LIBSTUD_REGEX_HAS_CPP_ATTRIBUTE(nodiscard)
#  define LIBSTUD_REGEX_NODISCARD [[nodiscard]]
#else
#  define LIBSTUD_REGEX_NODISCARD
#endif
#if LIBSTUD_REGEX_HAS_CPP_ATTRIBUTE(fallthrough)
#  define LIBSTUD_REGEX_FALLTHROUGH [[fallthrough]]
#else
#  define LIBSTUD_REGEX_FALLTHROUGH
#endif

// Note that the following macros are currently used only in tests copied from
// llvm-project/libcxx. Let's, however, define them here to keep all
// definitions in a single place, for convenience.
//

// Make sure that tests do not rely on libc++-specific features.
//
#undef LIBSTUD_REGEX_LIBCPP

#undef LIBSTUD_REGEX_ABI_MICROSOFT

#define LIBSTUD_REGEX_AVAILABILITY_HAS_FROM_CHARS_FLOATING_POINT 0
#define LIBSTUD_REGEX_HAS_UNICODE                                1
#define LIBSTUD_REGEX_HAS_EXPERIMENTAL_TZDB                      0

#if defined(_WIN32)
#  define LIBSTUD_REGEX_HAS_OPEN_WITH_WCHAR     1
#else
#  define LIBSTUD_REGEX_HAS_OPEN_WITH_WCHAR     0
#endif // defined(_WIN32)

#if !defined(__SIZEOF_INT128__) || defined(_MSC_VER)
#  define LIBSTUD_REGEX_HAS_INT128              0
#  define LIBSTUD_REGEX_HAS_NO_INT128           1
#else
#  define LIBSTUD_REGEX_HAS_INT128              1
#  undef  LIBSTUD_REGEX_HAS_NO_INT128
#endif

#define LIBSTUD_REGEX_HAS_THREADS               1
#undef  LIBSTUD_REGEX_HAS_NO_THREADS

#define LIBSTUD_REGEX_HAS_FILESYSTEM            1
#undef  LIBSTUD_REGEX_HAS_NO_FILESYSTEM

#define LIBSTUD_REGEX_HAS_C8RTOMB_MBRTOC8       0
#undef  LIBSTUD_REGEX_HAS_NO_C8RTOMB_MBRTOC8

#define LIBSTUD_REGEX_HAS_RANDOM_DEVICE         1
#undef  LIBSTUD_REGEX_HAS_NO_RANDOM_DEVICE

#define LIBSTUD_REGEX_HAS_TIME_ZONE_DATABASE    1
#undef  LIBSTUD_REGEX_HAS_NO_TIME_ZONE_DATABASE

// Delegate stud::regex::regex_traits::transform_primary() call to
// std::regex_traits::transform_primary(), if the latter implementation for
// the current platform is better than the one provided by libc++, which we
// fork from (see stud::regex::regex_traits::__transform_primary() for
// details).
//
#if defined(_WIN32) && defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION >= 145
#  define LIBSTUD_REGEX_DELEGATE_TO_STD_TRANSFORM_PRIMARY 1
#else
#  define LIBSTUD_REGEX_DELEGATE_TO_STD_TRANSFORM_PRIMARY 0
#endif
