// file      : odb/sqlite/version.hxx.in
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef LIBODB_SQLITE_VERSION // Note: using the version macro itself.

// New numeric version format is AAAAABBBBBCCCCCDDDE where:
//
// AAAAA - major version number
// BBBBB - minor version number
// CCCCC - bugfix version number
// DDD   - alpha / beta (DDD + 500) version number
// E     - final (0) / snapshot (1)
//
// When DDDE is not 0, 1 is subtracted from AAAAABBBBBCCCCC. For example:
//
// Version      AAAAABBBBBCCCCCDDDE
//
// 0.1.0        0000000001000000000
// 0.1.2        0000000001000020000
// 1.2.3        0000100002000030000
// 2.2.0-a.1    0000200001999990010
// 3.0.0-b.2    0000299999999995020
// 2.2.0-a.1.z  0000200001999990011
//
#define LIBODB_SQLITE_VERSION_FULL  200004999995270ULL
#define LIBODB_SQLITE_VERSION_STR   "2.5.0-b.27"
#define LIBODB_SQLITE_VERSION_ID    "2.5.0-b.27"

#define LIBODB_SQLITE_VERSION_MAJOR 2
#define LIBODB_SQLITE_VERSION_MINOR 5
#define LIBODB_SQLITE_VERSION_PATCH 0

#define LIBODB_SQLITE_PRE_RELEASE   true

#define LIBODB_SQLITE_SNAPSHOT      0ULL
#define LIBODB_SQLITE_SNAPSHOT_ID   ""

#include <odb/version.hxx>

#ifdef LIBODB_VERSION_FULL
#  if !(LIBODB_VERSION_FULL == 200004999995270ULL)
#    error incompatible libodb version, libodb == 2.5.0-b.27 is required
#  endif
#endif


// Old/deprecated numeric version format is AABBCCDD where:
//
// AA - major version number
// BB - minor version number
// CC - bugfix version number
// DD - alpha / beta (DD + 50) version number
//
// When DD is not 00, 1 is subtracted from AABBCC. For example:
//
// Version     AABBCCDD
// 2.0.0       02000000
// 2.1.0       02010000
// 2.1.1       02010100
// 2.2.0.a1    02019901
// 3.0.0.b2    02999952
//
#define LIBODB_SQLITE_VERSION 2049977

#endif // LIBODB_SQLITE_VERSION
