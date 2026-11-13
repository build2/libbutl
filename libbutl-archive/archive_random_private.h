/*
  Note: dummy stub (archive_random() is only used in archive_mktemp() which
  is unused.
 */

#ifndef ARCHIVE_RANDOM_PRIVATE_H_INCLUDED
#define ARCHIVE_RANDOM_PRIVATE_H_INCLUDED

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif

/* Random number generator. */
static inline int archive_random(void *buf, size_t nbytes)
{
  (void) buf;
  (void) nbytes;
  return ARCHIVE_FATAL;
}

#endif /* !ARCHIVE_RANDOM_PRIVATE_H_INCLUDED */
