/* file      : libpkg-config/config.h.in -*- C -*-
 * license   : ISC; see accompanying COPYING file
 */
#ifndef LIBPKG_CONFIG_CONFIG_H
#define LIBPKG_CONFIG_CONFIG_H

#if defined(__linux__)
#  include <features.h> /* __GLIBC__, __GLIBC_MINOR__ */
#endif

/*
 * strndup() is not present on Windows while strl*() are only present on
 * *BSD and MacOS.
 *
 */
#if !defined(_WIN32)
#  define HAVE_STRNDUP 1
#endif

/*
 * GNU libc added strlcpy() and strlcat() in version 2.38 (in anticipation
 * of their addition to POSIX).
 */
#if defined(__FreeBSD__) || \
    defined(__OpenBSD__) || \
    defined(__NetBSD__)  || \
    defined(__APPLE__)   || \
    (defined(__GLIBC__)       && \
     defined(__GLIBC_MINOR__) && \
     (__GLIBC__  > 2 || __GLIBC__ == 2 && __GLIBC_MINOR__ >= 38))
#  define HAVE_STRLCPY 1
#  define HAVE_STRLCAT 1
#endif

#define LIBPKG_CONFIG_PROJECT_URL "https://github.com/build2/libpkg-config"

#define PKG_CONFIG_DEFAULT_PATH ""
#define SYSTEM_INCLUDEDIR       ""
#define SYSTEM_LIBDIR           ""

#endif /* LIBPKG_CONFIG_CONFIG_H */
