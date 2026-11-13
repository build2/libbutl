#undef HAVE_ACL_SET_FD
#undef HAVE_ACL_SET_FD_NP
#undef HAVE_BZLIB_H
#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#define BUILD2_AUTOCONF_LIBC_VERSION

/* Include headers required to make various libc version number macros
 * available.
 *
 * Note that besides the platform-specific macros listed below, this
 * check also defines the following helper macros:
 *
 * BUILD2_AUTOCONF_GLIBC_PREREQ(maj, min)
 * BUILD2_AUTOCONF_FREEBSD_PREREQ(maj, min)
 * BUILD2_AUTOCONF_OPENBSD_PREREQ(yyyymm)
 * BUILD2_AUTOCONF_NETBSD_PREREQ(maj, min)
 * BUILD2_AUTOCONF_MACOS_PREREQ(maj, min)
 * BUILD2_AUTOCONF_MINGW_PREREQ(maj, min)
 *
 * Note that all of the above macros are always defined and evaluate to false
 * on platforms to which they do not apply. Which means that instead of:
 *
 * #if defined(__GLIBC__) && BUILD2_AUTOCONF_GLIBC_PREREQ(2, 3)
 *
 * You can just write:
 *
 * #if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 3)
 *
 * Note also that for checking just for the specific platform we still use one
 * of the system macros below. For example:
 *
 * #if defined(__GLIBC__)
 * #if defined(__MUSL__)
 * #if defined(__FreeBSD__)
 * #if defined(__OpenBSD__)
 * #if defined(__NetBSD__)
 * #if defined(__MINGW32__)
 *
 * Except for MacOS specifically, which we detect using our own macro (for
 * the sake of simplicity):
 *
 * #if defined(BUILD2_AUTOCONF_MACOS)
 *
 * If, however, you want to cover the entire Apple operation systems family,
 * then use:
 *
 * #if defined(__APPLE__)
 *
 * Macros for detecting platforms and their versions:
 *
 * __GLIBC__: The glibc major version number.
 *
 * __GLIBC_MINOR__: The glibc minor version number.
 *
 * __GLIBC_PREREQ(major,minor): Return 1 if a given version number is greater
 *                              than or equal to the current glibc version
 *                              (__GLIBC__.__GLIBC_MINOR__), or 0 otherwise.
 *
 * __MUSL__: Defined by build2 if the Musl libc is used. No version is
 *           available.
 *
 * __FreeBSD__: Defined if on FreeBSD. The value is the FreeBSD major version
 *              number but it's typically used only for OS detection. This is
 *              a predefined macro so no header needs to be included.
 *
 * __FreeBSD_version: The FreeBSD version in the following format: MMmmXXX
 *                    where `MM` is the major and `mm` the minor version
 *                    numbers. The `XXX` portion can be disregarded for the
 *                    purposes of feature checking (because it's too
 *                    fine-grained).
 *
 * __OpenBSD__: Defined if on OpenBSD. This is a predefined macro so no header
 *              needs to be included.
 *
 * OpenBSD: The OpenBSD version in the following format: YYYYMM (year and
 *          month of release). There are always two releases per year: one
 *          with an odd-numbered minor version earlier in the year and one
 *          with an even-numbered minor version later in the year. OpenBSD 7.0
 *          was released in October 2021 so its OpenBSD macro value is 202110.
 *
 *
 * __NetBSD__: Defined if on NetBSD. This is a predefined macro so no header
 *             needs to be included.
 *
 * __NetBSD_Version__: The NetBSD version in the following format: MMmmrrpp00
 *                     where `MM` is the major and `mm` the minor version
 *                     numbers, `rr` is always 0 after NetBSD version 2.0I,
 *                     and `pp` is the patch level. The `rrpp00` portion can
 *                     be disregarded for the purposes of feature checking
 *                     (because it's too fine-grained).
 *
 * __MAC_OS_X_VERSION_MIN_REQUIRED: The minimum supported Mac OS version. Up
 *                                  to Mac OS X 10.9, formatted as `MMm0`
 *                                  where `m` was the (always single-digit)
 *                                  minor version. Since Mac OS X 10.10 it is
 *                                  formatted as `MMmmpp` where `mm` is the
 *                                  (now potentially double-digit) minor
 *                                  version and `pp` is the patch version.
 *
 * __MINGW32__: Defined if on original Mingw and on Mingw-w64, both 32 and
 *              64-bit. This is a predefined macro so no header needs to be
 *              included. Note that we don't really care about original Mingw.
 *
 * __MINGW64__: Defined if on Mingw-w64, 64-bit only. This is a predefined
 *              macro so no header needs to be included.
 *
 * __MINGW64_VERSION_MAJOR:
 * __MINGW64_VERSION_MINOR: The Mingw-w64 major/minor version numbers. Note
 *                          that these are defined for both 32 and 64-bit.
 */
#if defined(__linux__)
#  include <features.h> /* __GLIBC__, __GLIBC_MINOR__, __GLIBC_PREREQ() */
#elif defined(__FreeBSD__)
#  include <osreldate.h> /* __FreeBSD_version */
#elif defined(__OpenBSD__) || defined(__NetBSD__)
/* Note: sys/param.h defines macros with common names such as ALIGN which may
 * clash with user code (see Qt).
 */
#  include <sys/param.h> /* OpenBSD, __NetBSD_Version__ */
#elif defined(__APPLE__)
/* Note: some Apple headers use macros like __has_extension and __has_buildin
 * without checking whether they are defined. This causes issues with GCC
 * which does not provide them yet (see GCC bug 90835). It happens that the
 * <sys/cdefs.h> header provides the necessary fallbacks.
 */
#  include <sys/cdefs.h>
#  include <Availability.h> /* __MAC_OS_X_VERSION_MIN_REQUIRED */
#elif defined(__MINGW32__)
#  include <_mingw.h> /* __MINGW64_VERSION_{MAJOR,MINOR} */
#endif

/* BUILD2_AUTOCONF_GLIBC_PREREQ(maj, min)
 *
 * Return 1 if the glibc version is >= the given version number, or 0
 * otherwise.
 *
 * __GLIBC_PREREQ() first appeared in glibc 2.2 (released in 2000) so define
 * BUILD2_AUTOCONF_GLIBC_PREREQ() using its latest glibc implementation if it
 * is not defined.
 *
 * __GLIBC__ and __GLIBC_MINOR__ first appeared in glibc 1.97 on 1996-10-22
 * (but only released in glibc 2.0 on 1997-01-26) with no preceding equivalent
 * so always return 0 if these macros are not defined.
 */
#if defined(__GLIBC__) && defined(__GLIBC_MINOR__)
#  if defined(__GLIBC_PREREQ)
#    define BUILD2_AUTOCONF_GLIBC_PREREQ(maj, min) __GLIBC_PREREQ(maj, min)
#  else
#    define BUILD2_AUTOCONF_GLIBC_PREREQ(maj, min) \
       ((__GLIBC__ << 16) + __GLIBC_MINOR__ >= ((maj) << 16) + (min))
#  endif
#else
#  define BUILD2_AUTOCONF_GLIBC_PREREQ(maj, min) 0
#endif

/* BUILD2_AUTOCONF_FREEBSD_PREREQ(maj, min)
 *
 * Return 1 if the FreeBSD version is >= the given version number, or 0
 * otherwise.
 */
#if defined(__FreeBSD__) && defined(__FreeBSD_version)
#  define BUILD2_AUTOCONF_FREEBSD_PREREQ(maj, min) \
     (__FreeBSD_version >= (maj)*100000 + (min)*1000)
#else
#  define BUILD2_AUTOCONF_FREEBSD_PREREQ(maj, min) 0
#endif

/* BUILD2_AUTOCONF_OPENBSD_PREREQ(yyyymm)
 *
 * Return 1 if the release date of the OpenBSD version is >= the given release
 * date, or 0 otherwise.
 */
#if defined(__OpenBSD__) && defined(OpenBSD)
#  define BUILD2_AUTOCONF_OPENBSD_PREREQ(yyyymm) (OpenBSD >= (yyyymm))
#else
#  define BUILD2_AUTOCONF_OPENBSD_PREREQ(yyyymm) 0
#endif

/* BUILD2_AUTOCONF_NETBSD_PREREQ(maj, min)
 *
 * Return 1 if the NetBSD version is >= the given version number, or 0
 * otherwise.
 */
#if defined(__NetBSD__) && defined(__NetBSD_Version__)
#  define BUILD2_AUTOCONF_NETBSD_PREREQ(maj, min) \
     (__NetBSD_Version__ >= (maj)*100000000 + (min)*1000000)
#else
#  define BUILD2_AUTOCONF_NETBSD_PREREQ(maj, min) 0
#endif

/* BUILD2_AUTOCONF_MACOS_PREREQ(maj, min)
 *
 * Return 1 if the Mac OS version is >= the given version number, or 0
 * otherwise.
 *
 * BUILD2_AUTOCONF_MACOS
 *
 * Defined if the current platform is MacOS, or undefined otherwise.
 */
#if defined(__APPLE__) && defined(__MACH__)
/* Note that just __APPLE__ covers all the Apple platforms (MacOS, iOS, etc).
 */
#  define BUILD2_AUTOCONF_MACOS 1

#  if !defined(__MAC_OS_X_VERSION_MIN_REQUIRED)
#    error __MAC_OS_X_VERSION_MIN_REQUIRED not defined
#  endif
#  if __MAC_OS_X_VERSION_MIN_REQUIRED <= 1090
#    define BUILD2_AUTOCONF_MACOS_PREREQ(maj, min) \
       (__MAC_OS_X_VERSION_MIN_REQUIRED >= (maj)*100 + (min)*10)
#  else
#    define BUILD2_AUTOCONF_MACOS_PREREQ(maj, min) \
       (__MAC_OS_X_VERSION_MIN_REQUIRED >= (maj)*10000 + (min)*100)
#  endif
#else
#  undef BUILD2_AUTOCONF_MACOS

#  define BUILD2_AUTOCONF_MACOS_PREREQ(maj, min) 0
#endif

/* BUILD2_AUTOCONF_MINGW_PREREQ(maj, min)
 *
 * Return 1 if the Mingw-w64 version is >= the given version number, or 0
 * otherwise.
 */
#if defined(__MINGW64_VERSION_MAJOR) && defined(__MINGW64_VERSION_MINOR)
#  define BUILD2_AUTOCONF_MINGW_PREREQ(maj, min)  \
            (__MINGW64_VERSION_MAJOR > (maj) ||   \
             (__MINGW64_VERSION_MAJOR == (maj) && \
              __MINGW64_VERSION_MINOR >= (min)))
#else
#  define BUILD2_AUTOCONF_MINGW_PREREQ(maj, min) 0
#endif

#endif /* BUILD2_AUTOCONF_LIBC_VERSION */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_INT32_MAX

/* Check for the INT32_MAX macro defined in <stdint.h>.
 *
 * Available since Linux/glibc 2.0, OpenBSD 2.0, FreeBSD 5.0, NetBSD 1.6, Mac
 * OS and Windows (same as HAVE_STDINT_H).
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 0) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(5, 0) || \
    BUILD2_AUTOCONF_OPENBSD_PREREQ(199610) || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(1, 6) || \
    defined(BUILD2_AUTOCONF_MACOS) || \
    defined(_WIN32)
#  define HAVE_INT32_MAX 1
#endif

#undef HAVE_DECL_INT32_MAX
#ifdef HAVE_INT32_MAX
#  define HAVE_DECL_INT32_MAX HAVE_INT32_MAX
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_INT32_MIN

/* Check for the INT32_MIN macro defined in <stdint.h>.
 *
 * Available since Linux/glibc 2.0, OpenBSD 2.0, FreeBSD 5.0, NetBSD 1.6, Mac
 * OS and Windows (same as HAVE_STDINT_H).
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 0) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(5, 0) || \
    BUILD2_AUTOCONF_OPENBSD_PREREQ(199610) || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(1, 6) || \
    defined(BUILD2_AUTOCONF_MACOS) || \
    defined(_WIN32)
#  define HAVE_INT32_MIN 1
#endif

#undef HAVE_DECL_INT32_MIN
#ifdef HAVE_INT32_MIN
#  define HAVE_DECL_INT32_MIN HAVE_INT32_MIN
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_INT64_MAX

/* Check for the INT64_MAX macro defined in <stdint.h>.
 *
 * Available since Linux/glibc 2.0, OpenBSD 2.0, FreeBSD 5.0, NetBSD 1.6, Mac
 * OS and Windows (same as HAVE_STDINT_H).
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 0) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(5, 0) || \
    BUILD2_AUTOCONF_OPENBSD_PREREQ(199610) || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(1, 6) || \
    defined(BUILD2_AUTOCONF_MACOS) || \
    defined(_WIN32)
#  define HAVE_INT64_MAX 1
#endif

#undef HAVE_DECL_INT64_MAX
#ifdef HAVE_INT64_MAX
#  define HAVE_DECL_INT64_MAX HAVE_INT64_MAX
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_INT64_MIN

/* Check for the INT64_MIN macro defined in <stdint.h>.
 *
 * Available since Linux/glibc 2.0, OpenBSD 2.0, FreeBSD 5.0, NetBSD 1.6, Mac
 * OS and Windows (same as HAVE_STDINT_H).
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 0) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(5, 0) || \
    BUILD2_AUTOCONF_OPENBSD_PREREQ(199610) || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(1, 6) || \
    defined(BUILD2_AUTOCONF_MACOS) || \
    defined(_WIN32)
#  define HAVE_INT64_MIN 1
#endif

#undef HAVE_DECL_INT64_MIN
#ifdef HAVE_INT64_MIN
#  define HAVE_DECL_INT64_MIN HAVE_INT64_MIN
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_INTMAX_MAX

/* Check for the INTMAX_MAX macro defined in <stdint.h>.
 *
 * Available since Linux/glibc 2.0, OpenBSD 2.0, FreeBSD 5.0, NetBSD 1.6, Mac
 * OS and Windows (same as HAVE_STDINT_H).
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 0) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(5, 0) || \
    BUILD2_AUTOCONF_OPENBSD_PREREQ(199610) || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(1, 6) || \
    defined(BUILD2_AUTOCONF_MACOS) || \
    defined(_WIN32)
#  define HAVE_INTMAX_MAX 1
#endif

#undef HAVE_DECL_INTMAX_MAX
#ifdef HAVE_INTMAX_MAX
#  define HAVE_DECL_INTMAX_MAX HAVE_INTMAX_MAX
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_INTMAX_MIN

/* Check for the INTMAX_MIN macro defined in <stdint.h>.
 *
 * Available since Linux/glibc 2.0, OpenBSD 2.0, FreeBSD 5.0, NetBSD 1.6, Mac
 * OS and Windows (same as HAVE_STDINT_H).
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 0) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(5, 0) || \
    BUILD2_AUTOCONF_OPENBSD_PREREQ(199610) || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(1, 6) || \
    defined(BUILD2_AUTOCONF_MACOS) || \
    defined(_WIN32)
#  define HAVE_INTMAX_MIN 1
#endif

#undef HAVE_DECL_INTMAX_MIN
#ifdef HAVE_INTMAX_MIN
#  define HAVE_DECL_INTMAX_MIN HAVE_INTMAX_MIN
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_SIZE_MAX

/* Check for the SIZE_MAX macro defined in <stdint.h>.
 *
 * Available since Linux/glibc 2.0, OpenBSD 2.0, FreeBSD 5.0, NetBSD 1.6, Mac
 * OS and Windows (same as HAVE_STDINT_H).
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 0) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(5, 0) || \
    BUILD2_AUTOCONF_OPENBSD_PREREQ(199610) || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(1, 6) || \
    defined(BUILD2_AUTOCONF_MACOS) || \
    defined(_WIN32)
#  define HAVE_SIZE_MAX 1
#endif

#undef HAVE_DECL_SIZE_MAX
#ifdef HAVE_SIZE_MAX
#  define HAVE_DECL_SIZE_MAX HAVE_SIZE_MAX
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_SSIZE_MAX

/* Check for the SSIZE_MAX macro defined in <limits.h> on some platforms.
 *
 * Available on Linux with POSIX, FreeBSD, and Mac OS (will assume always).
 * Not available on Windows except on MinGW. NetBSD and OpenBSD are TODO.
 */
#if defined(__linux__)
#  if defined(__USE_POSIX)
#    define HAVE_SSIZE_MAX 1
#  endif
#elif defined(__FreeBSD__)
#  define HAVE_SSIZE_MAX 1
#elif defined(_WIN32)
#  if defined(__MINGW32__)
#    define HAVE_SSIZE_MAX 1
#  endif
#endif

#undef HAVE_DECL_SSIZE_MAX
#ifdef HAVE_SSIZE_MAX
#  define HAVE_DECL_SSIZE_MAX HAVE_SSIZE_MAX
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_UINT32_MAX

/* Check for the UINT32_MAX macro defined in <stdint.h>.
 *
 * Available since Linux/glibc 2.0, OpenBSD 2.0, FreeBSD 5.0, NetBSD 1.6, Mac
 * OS and Windows (same as HAVE_STDINT_H).
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 0) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(5, 0) || \
    BUILD2_AUTOCONF_OPENBSD_PREREQ(199610) || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(1, 6) || \
    defined(BUILD2_AUTOCONF_MACOS) || \
    defined(_WIN32)
#  define HAVE_UINT32_MAX 1
#endif

#undef HAVE_DECL_UINT32_MAX
#ifdef HAVE_UINT32_MAX
#  define HAVE_DECL_UINT32_MAX HAVE_UINT32_MAX
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_UINT64_MAX

/* Check for the UINT64_MAX macro defined in <stdint.h>.
 *
 * Available since Linux/glibc 2.0, OpenBSD 2.0, FreeBSD 5.0, NetBSD 1.6, Mac
 * OS and Windows (same as HAVE_STDINT_H).
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 0) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(5, 0) || \
    BUILD2_AUTOCONF_OPENBSD_PREREQ(199610) || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(1, 6) || \
    defined(BUILD2_AUTOCONF_MACOS) || \
    defined(_WIN32)
#  define HAVE_UINT64_MAX 1
#endif

#undef HAVE_DECL_UINT64_MAX
#ifdef HAVE_UINT64_MAX
#  define HAVE_DECL_UINT64_MAX HAVE_UINT64_MAX
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_UINTMAX_MAX

/* Check for the UINTMAX_MAX macro defined in <stdint.h>.
 *
 * Available since Linux/glibc 2.0, OpenBSD 2.0, FreeBSD 5.0, NetBSD 1.6, Mac
 * OS and Windows (same as HAVE_STDINT_H).
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 0) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(5, 0) || \
    BUILD2_AUTOCONF_OPENBSD_PREREQ(199610) || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(1, 6) || \
    defined(BUILD2_AUTOCONF_MACOS) || \
    defined(_WIN32)
#  define HAVE_UINTMAX_MAX 1
#endif

#undef HAVE_DECL_UINTMAX_MAX
#ifdef HAVE_UINTMAX_MAX
#  define HAVE_DECL_UINTMAX_MAX HAVE_UINTMAX_MAX
#endif
#undef HAVE_EFTYPE

/* Check for the EFTYPE errno code.
 *
 * Only defined on BSDs and Mac OS (including variants like iOS).
 */
#if defined(__FreeBSD__) || \
    defined(__OpenBSD__) || \
    defined(__NetBSD__)  || \
    defined(__APPLE__)
#  define HAVE_EFTYPE 1
#endif
#undef HAVE_EILSEQ

/* Check for the EILSEQ errno code.
 *
 * Part of C95 and appears to be available everywhere.
 */
#define HAVE_EILSEQ 1
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_ERRNO_H

/* Since Linux/glibc 2.1, OpenBSD 2.0, FreeBSD 1.0, NetBSD 1.0, Mac OS
 * and win32.
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 1) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(1, 0) || \
    BUILD2_AUTOCONF_OPENBSD_PREREQ(199610) || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(1, 0) || \
    defined(BUILD2_AUTOCONF_MACOS) || \
    defined(_WIN32)
#  define HAVE_ERRNO_H 1
#endif
#undef HAVE_EXT2FS_EXT2_FS_H
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_FCHMOD

/* Check for the fchmod() function.
 *
 * Available since glibc 2.10 but is only enabled if certain macros are
 * defined (see chmod(2) for details). For now we only cover 2.16 and up and
 * _POSIX_C_SOURCE. Assume available in all BSD and Apple OS versions we care
 * to target (reportedly available since 4.2BSD). Not available on Windows
 * (including MinGW).
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 24)
#  if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L
#    define HAVE_FCHMOD 1
#  endif
#elif BUILD2_AUTOCONF_GLIBC_PREREQ(2, 16)
#  if defined(_POSIX_C_SOURCE)
#    define HAVE_FCHMOD 1
#  endif
#elif defined(__FreeBSD__) || \
      defined(__OpenBSD__) || \
      defined(__NetBSD__)  || \
      defined(__APPLE__)
#  define HAVE_FCHMOD 1
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_FCHOWN

/* Check for the fchown() function.
 *
 * Available since glibc 2.12 but is only enabled if certain macros are
 * defined (see chown(2) for details). For now we only cover _POSIX_C_SOURCE.
 * Assume available in all BSD and Apple OS versions we care to target
 * (reportedly available since 4.2BSD). Not available on Windows (including
 * MinGW).
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 12)
#  if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L
#    define HAVE_FCHOWN 1
#  endif
#elif defined(__FreeBSD__) || \
      defined(__OpenBSD__) || \
      defined(__NetBSD__)  || \
      defined(__APPLE__)
#  define HAVE_FCHOWN 1
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_FCNTL_H

/* Note that there is <fcntl.h> on Windows (both MSVC and MinGW) though its
 * declarations are not the same as on POSIX. But it does exist.
 */

#if defined(__linux__)              || \
    defined(__FreeBSD__)            || \
    defined(__OpenBSD__)            || \
    defined(__NetBSD__)             || \
    defined(BUILD2_AUTOCONF_MACOS)  || \
    defined(_WIN32)
#  define HAVE_FCNTL_H 1
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_FUTIMES

/* Since Linux/glibc 2.3, FreeBSD 3.x, OpenBSD 1.2 (but the OpenBSD macro
 * was only added in 2.0 with a value of 199610), NetBSD 1.2, Mac OS (all
 * versions).
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 3)     || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(3, 0)   || \
    BUILD2_AUTOCONF_OPENBSD_PREREQ(199610) || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(1, 2)    || \
    defined(BUILD2_AUTOCONF_MACOS)
#  define HAVE_FUTIMES 1
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_GETEGID

/* Since Linux/glibc 2.4, OpenBSD 2.0, FreeBSD 1.0, NetBSD 1.0, and Mac OS
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 4) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(1, 0)|| \
    BUILD2_AUTOCONF_OPENBSD_PREREQ(199610) || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(1, 0) || \
    defined(BUILD2_AUTOCONF_MACOS)
#  define HAVE_GETEGID 1
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_GETEUID

/* Since Linux/glibc 2.4, OpenBSD 2.0, FreeBSD 1.0, NetBSD 1.0, and Mac OS
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 4) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(1, 0)|| \
    BUILD2_AUTOCONF_OPENBSD_PREREQ(199610) || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(1, 0) || \
    defined(BUILD2_AUTOCONF_MACOS)
#  define HAVE_GETEUID 1
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_GETRESUID

/* Check for the getresuid() function from the unistd.h header.
 *
 * Available since glibc 2.3.2 with _GNU_SOURCE, FreeBSD 4.0. Not available
 * on Mac OS or Windows.
 */
#if (BUILD2_AUTOCONF_GLIBC_PREREQ(2, 3) && defined(_GNU_SOURCE)) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(4, 0)
#  define HAVE_GETRESUID 1
#endif
#undef HAVE_ICONV
#undef HAVE_ICONV_H
#undef HAVE_INTSAFE_H

/* Check for the intsafe.h header.
 *
 * Only available on Windows.
 */
#if defined(_WIN32)
#  define HAVE_INTSAFE_H 1
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_INTTYPES_H

/* Since OpenBSD 3.9, FreeBSD 5.0, NetBSD 5.0, Mac OS X 10.3
 * glibc 2.2, Mingw-w64 2.0, Platform SDK 10.
 */
#if BUILD2_AUTOCONF_OPENBSD_PREREQ(200601) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(2, 3)   || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(5, 0)    || \
    BUILD2_AUTOCONF_MACOS_PREREQ(10, 3)    || \
    BUILD2_AUTOCONF_GLIBC_PREREQ(2, 2)     || \
    BUILD2_AUTOCONF_MINGW_PREREQ(2, 0)     || \
    defined(_MSC_VER)
#  define HAVE_INTTYPES_H 1
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_ISSETUGID

/* Since OpenBSD 2.0, FreeBSD 3.0, NetBSD 1.5, Mac OS X, Solaris
 */
#if BUILD2_AUTOCONF_OPENBSD_PREREQ(200601) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(2, 3)   || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(5, 0)    || \
    defined(BUILD2_AUTOCONF_MACOS)         || \
    ((defined(__sun) && defined(__SVR4)) || defined(__sun__))
#  define HAVE_ISSETUGID 1
#endif
#undef HAVE_LANGINFO_H

/* Check for the langinfo.h header.
 *
 * Assume available everywhere except Windows (not even MinGW). See also
 * HAVE_NL_LANGINFO.
 */
#ifndef _WIN32
#  define HAVE_LANGINFO_H 1
#endif
#undef HAVE_LIMITS_H

/* Check for the limits.h header.
 *
 * Assume available everywhere.
 */
#define HAVE_LIMITS_H 1
#undef HAVE_LINUX_EXT2_FS_H
#undef HAVE_LINUX_FS_H
#undef HAVE_LOCALCHARSET_H
#undef HAVE_LOCALE_CHARSET
#undef HAVE_LZ4_H
#undef HAVE_LZMA_H
#undef HAVE_MBRTOWC

/* Check for the mbrtowc() function from the wchar.h header.
 *
 * Assume available everywhere (the same as HAVE_WCHAR_H).
 */
#define HAVE_MBRTOWC 1
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_MKSTEMP

/* Check for the mkstemp() function.
 *
 * glibc; 4.3BSD and thus FreeBSD, OpenBSD, NetBSD, and Mac OS X since the
 * beginning; Mingw-w64 4.0. Not available on Windows (except Mingw-w64).
 */
#if defined(__GLIBC__)                 || \
    defined(__FreeBSD__)               || \
    defined(__NetBSD__)                || \
    defined(__OpenBSD__)               || \
    BUILD2_AUTOCONF_MINGW_PREREQ(4, 0) || \
    defined(BUILD2_AUTOCONF_MACOS)
#  define HAVE_MKSTEMP 1
#endif
#undef HAVE_NL_LANGINFO

/* Check for the ln_langinfo() function.
 *
 * Assume available everywhere except Windows (not even MinGW). See also
 * HAVE_LANGINFO_H.
 */
#ifndef _WIN32
#  define HAVE_NL_LANGINFO 1
#endif
#undef HAVE_STDARG_H

#define HAVE_STDARG_H 1
#undef HAVE_STDCKDINT_H

/* Check for the stdckdint.h header.
 *
 * Availabe since C23 and C++26 as the same header name.
 */
#if defined(__cplusplus)
#  if __cplusplus >= 202603L
#    define HAVE_STDCKDINT_H 1
#  endif
#elif defined(__STDC_VERSION__)
#  if __STDC_VERSION__ >= 202311L
#    define HAVE_STDCKDINT_H 1
#  endif
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_STDINT_H

/* Since Linux/glibc 2.0, OpenBSD 2.0, FreeBSD 5.0, NetBSD 1.6, Mac OS
 * and win32.
 */
#if BUILD2_AUTOCONF_GLIBC_PREREQ(2, 0) || \
    BUILD2_AUTOCONF_FREEBSD_PREREQ(5, 0) || \
    BUILD2_AUTOCONF_OPENBSD_PREREQ(199610) || \
    BUILD2_AUTOCONF_NETBSD_PREREQ(1, 6) || \
    defined(BUILD2_AUTOCONF_MACOS) || \
    defined(_WIN32)
#  define HAVE_STDINT_H 1
#endif
#undef HAVE_STDLIB_H

#define HAVE_STDLIB_H 1
#undef HAVE_STRING_H

/* Check for the string.h header.
 *
 * Assume available everywhere.
 */
#define HAVE_STRING_H 1
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_SYS_STAT_H
/* Since 4.2BSD (~1983)
 */
#if defined(__linux__)              || \
    defined(__FreeBSD__)            || \
    defined(__OpenBSD__)            || \
    defined(__NetBSD__)             || \
    defined(BUILD2_AUTOCONF_MACOS)  || \
    defined(_WIN32)
#  define HAVE_SYS_STAT_H 1
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_SYS_TYPES_H
/* Linux only (?) */
#if defined(__linux__)
#  define HAVE_SYS_TYPES_H 1
#endif
#undef HAVE_SYS_UTIME_H

/* Check for the sys/utime.h header.
 *
 * Only available on Windows.
 */
#if defined(_WIN32)
#  define HAVE_SYS_UTIME_H 1
#endif
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_SYS_WAIT_H
/* Since 4.3BSD (~1994)
 */
#if defined(__linux__)   || \
    defined(__FreeBSD__) || \
    defined(__OpenBSD__) || \
    defined(__NetBSD__)  || \
    defined(BUILD2_AUTOCONF_MACOS)
#  define HAVE_SYS_WAIT_H 1
#endif
#undef HAVE_TIME_H

/* Check for the time.h header.
 *
 * Assume available everywhere.
 */
#define HAVE_TIME_H 1
/* Base BUILD2_AUTOCONF_LIBC_VERSION already defined. */


#ifndef BUILD2_AUTOCONF_LIBC_VERSION
#  error BUILD2_AUTOCONF_LIBC_VERSION appears to be conditionally included
#endif

#undef HAVE_UNISTD_H

/* Check for the <unistd.h> header.
 *
 * Available on Linux/glibc, FreeBSD, OpenBSD, NetBSD, and MacOS.
 * Not available on Windows except MinGW.
 */
#if defined(__linux__)   || \
    defined(__FreeBSD__) || \
    defined(__OpenBSD__) || \
    defined(__NetBSD__)  || \
    defined(__MINGW32__) || \
    defined(BUILD2_AUTOCONF_MACOS)
#  define HAVE_UNISTD_H 1
#endif
#undef HAVE_WCHAR_H

/* Check for the wchar.h header.
 *
 * Assume available everywhere.
 */
#define HAVE_WCHAR_H 1
#undef HAVE_WCRTOMB

/* Check for the wcrtomb() function from the wchar.h header.
 *
 * Assume available everywhere (the same as HAVE_WCHAR_H).
 */
#define HAVE_WCRTOMB 1
#undef HAVE_WCSLEN

/* Check for the wcslen() function from the wchar.h header.
 *
 * Assume available everywhere (the same as HAVE_WCHAR_H).
 */
#define HAVE_WCSLEN 1
#undef HAVE_WCTOMB

/* Check for the wctomb() function from the stdlib.h header.
 *
 * Assume available everywhere (C99).
 */
#define HAVE_WCTOMB 1
#undef HAVE_WMEMCMP

/* Check for the wmemcmp() function from the wchar.h header.
 *
 * Assume available everywhere (the same as HAVE_WCHAR_H).
 */
#define HAVE_WMEMCMP 1
#undef HAVE_WMEMCPY

/* Check for the wmemcpy() function from the wchar.h header.
 *
 * Assume available everywhere (the same as HAVE_WCHAR_H).
 */
#define HAVE_WMEMCPY 1
#undef HAVE_WMEMMOVE

/* Check for the wmemmove() function from the wchar.h header.
 *
 * Assume available everywhere (the same as HAVE_WCHAR_H).
 */
#define HAVE_WMEMMOVE 1
#define HAVE_ZLIB_H 1

/* If ssize_t is already defined, assume it's correct and use it as-is.
 *
 * Note that we may not be able to redefine it because while we can undef the
 * macro, there is no guarantee we will be able to re-include the header (due
 * to include guards).
 */
#ifndef ssize_t
  /* POSIX and MinGW (which also has <sys/types.h> that defines ssize_t).
   */
#  if defined(__linux__)                  || \
      defined(__FreeBSD__)                || \
      defined(__OpenBSD__)                || \
      defined(__NetBSD__)                 || \
      defined(__APPLE__)                  || \
      defined(__MINGW32__)                || \
      (defined(__sun) && defined(__SVR4)) || \
      defined(__CYGWIN__)
#    include <sys/types.h>
     /* If available, we do nothing. */
#  elif defined(_WIN32)
#    include <basetsd.h>
     /* Use define instead of typedef to avoid conflicts. */
#    define ssize_t SSIZE_T
#  else
     /* Else define it to int (suggested fallback by libevent). */
#    define ssize_t int
#  endif
#endif

#ifdef _WIN32
typedef int pid_t;
typedef short uid_t;
typedef short gid_t;
typedef unsigned short mode_t;
#endif
