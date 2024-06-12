#ifndef LIBPKG_CONFIG_VERSION_H
#define LIBPKG_CONFIG_VERSION_H

/* The numeric version format is AAAAABBBBBCCCCCDDDE where:
 *
 * AAAAA - major version number
 * BBBBB - minor version number
 * CCCCC - bugfix version number
 * DDD   - alpha / beta (DDD + 500) version number
 * E     - final (0) / snapshot (1)
 *
 * When DDDE is not 0, 1 is subtracted from AAAAABBBBBCCCCC. For example:
 *
 * Version      AAAAABBBBBCCCCCDDDE
 *
 * 0.1.0        0000000001000000000
 * 0.1.2        0000000001000020000
 * 1.2.3        0000100002000030000
 * 2.2.0-a.1    0000200001999990010
 * 3.0.0-b.2    0000299999999995020
 * 2.2.0-a.1.z  0000200001999990011
 */
#define LIBPKG_CONFIG_VERSION       1000020000ULL
#define LIBPKG_CONFIG_VERSION_STR   "0.1.2"
#define LIBPKG_CONFIG_VERSION_ID    "0.1.2"
#define LIBPKG_CONFIG_VERSION_FULL  "0.1.2"

#define LIBPKG_CONFIG_VERSION_MAJOR 0
#define LIBPKG_CONFIG_VERSION_MINOR 1
#define LIBPKG_CONFIG_VERSION_PATCH 2

#define LIBPKG_CONFIG_PRE_RELEASE   false

#define LIBPKG_CONFIG_SNAPSHOT_SN   0ULL
#define LIBPKG_CONFIG_SNAPSHOT_ID   ""

#endif /* LIBPKG_CONFIG_VERSION_H */
