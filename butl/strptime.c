/*-
 * Copyright (c) 2014 Gary Mills
 * Copyright 2011, Nexenta Systems, Inc.  All rights reserved.
 * Copyright (c) 1994 Powerdog Industries.  All rights reserved.
 *
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 * Portions of this software were developed by David Chisnall
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY POWERDOG INDUSTRIES ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE POWERDOG INDUSTRIES BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of Powerdog Industries.
 */

// Fallback implementation of strptime() designed for Windows/gcc combination
// when neither POSIX strptime() nor proper std::get_time() are available. No
// locale support is provided, so the function works as if "C" locale is set.
// FreeBSD libc strptime.c source file is taken as a basis (saved in
// strptime.orig). POSIX non-compliant extensions are removed. Most of
// #include directives are removed as reference internal headers, some
// #define directives are added instead. The content of included
// timelocal.{h,c} files is mostly commented out, having just required
// lc_time_T struct and _C_time_locale constant definitions left. Otherwise the
// code is kept untouched as much as possible.
//

#include <ctype.h>   // isspace(), isdigit()
#include <string.h>  // _strnicmp()

#include "timelocal.h" // lc_time_T
#include "timelocal.c" // _C_time_locale

#define isspace_l(c, l)             isspace(c)
#define isdigit_l(c, l)             isdigit(c)
#define strncasecmp_l(s1, s2, n, l) _strnicmp(s1, s2, n)

#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)
#define FIX_LOCALE(l) l=l

#define TM_YEAR_BASE	1900
#define TM_SUNDAY	0
#define TM_MONDAY	1

typedef unsigned char u_char;

// Stubs replacing the real locale support.
//
typedef const lc_time_T* locale_t;

static locale_t
__get_locale ()
{
  return &_C_time_locale;
}

static const struct lc_time_T *
__get_current_time_locale (locale_t l)
{
  return l;
}

// From this point the code is unchanged, if not to count non-standard
// specifiers support removal and the replacement of tabs with spaces.
//
static char *
_strptime(const char *, const char *, struct tm *, int *, locale_t);

#define	asizeof(a)	((int)(sizeof(a) / sizeof((a)[0])))

#define	FLAG_NONE	(1 << 0)
#define	FLAG_YEAR	(1 << 1)
#define	FLAG_MONTH	(1 << 2)
#define	FLAG_YDAY	(1 << 3)
#define	FLAG_MDAY	(1 << 4)
#define	FLAG_WDAY	(1 << 5)

/*
 * Calculate the week day of the first day of a year. Valid for
 * the Gregorian calendar, which began Sept 14, 1752 in the UK
 * and its colonies. Ref:
 * http://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week
 */

static int
first_wday_of(int year)
{
	return (((2 * (3 - (year / 100) % 4)) + (year % 100) +
		((year % 100) / 4) + (isleap(year) ? 6 : 0) + 1) % 7);
}

// Remove Glibc extensions (%F, %Z, %z, %s) to make implementation compliant
// with POSIX strptime() and to simplify porting.
//
static char *
_strptime(const char *buf, const char *fmt, struct tm *tm, int *GMTp,
		locale_t locale)
{
	char	c;
	const char *ptr;
	int	day_offset = -1, wday_offset;
	int week_offset;
	int	i, len;
	int flags;
	int Ealternative, Oalternative;
	const struct lc_time_T *tptr = __get_current_time_locale(locale);
	static int start_of_month[2][13] = {
		{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
		{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}
	};

	flags = FLAG_NONE;

	ptr = fmt;
	while (*ptr != 0) {
		c = *ptr++;

		if (c != '%') {
			if (isspace_l((unsigned char)c, locale))
				while (*buf != 0 &&
				       isspace_l((unsigned char)*buf, locale))
					buf++;
			else if (c != *buf++)
				return (NULL);
			continue;
		}

    // Skip according to
    // http://pubs.opengroup.org/onlinepubs/9699919799/functions/strptime.html
    //
    c = *ptr;
    if (c == '+' || c == '0')
      ptr++;

		Ealternative = 0;
		Oalternative = 0;
label:
		c = *ptr++;
		switch (c) {
		case '%':
			if (*buf++ != '%')
				return (NULL);
			break;

		case 'C':
			if (!isdigit_l((unsigned char)*buf, locale))
				return (NULL);

			/* XXX This will break for 3-digit centuries. */
			len = 2;
			for (i = 0; len && *buf != 0 &&
			     isdigit_l((unsigned char)*buf, locale); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i < 19)
				return (NULL);

			tm->tm_year = i * 100 - TM_YEAR_BASE;
			flags |= FLAG_YEAR;

			break;

		case 'c':
			buf = _strptime(buf, tptr->c_fmt, tm, GMTp, locale);
			if (buf == NULL)
				return (NULL);
			flags |= FLAG_WDAY | FLAG_MONTH | FLAG_MDAY | FLAG_YEAR;
			break;

		case 'D':
			buf = _strptime(buf, "%m/%d/%y", tm, GMTp, locale);
			if (buf == NULL)
				return (NULL);
			flags |= FLAG_MONTH | FLAG_MDAY | FLAG_YEAR;
			break;

		case 'E':
			if (Ealternative || Oalternative)
				break;
			Ealternative++;
			goto label;

		case 'O':
			if (Ealternative || Oalternative)
				break;
			Oalternative++;
			goto label;

		case 'R':
			buf = _strptime(buf, "%H:%M", tm, GMTp, locale);
			if (buf == NULL)
				return (NULL);
			break;

		case 'r':
			buf = _strptime(buf, tptr->ampm_fmt, tm, GMTp, locale);
			if (buf == NULL)
				return (NULL);
			break;

		case 'T':
			buf = _strptime(buf, "%H:%M:%S", tm, GMTp, locale);
			if (buf == NULL)
				return (NULL);
			break;

		case 'X':
			buf = _strptime(buf, tptr->X_fmt, tm, GMTp, locale);
			if (buf == NULL)
				return (NULL);
			break;

		case 'x':
			buf = _strptime(buf, tptr->x_fmt, tm, GMTp, locale);
			if (buf == NULL)
				return (NULL);
			flags |= FLAG_MONTH | FLAG_MDAY | FLAG_YEAR;
			break;

		case 'j':
			if (!isdigit_l((unsigned char)*buf, locale))
				return (NULL);

			len = 3;
			for (i = 0; len && *buf != 0 &&
			     isdigit_l((unsigned char)*buf, locale); buf++){
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i < 1 || i > 366)
				return (NULL);

			tm->tm_yday = i - 1;
			flags |= FLAG_YDAY;

			break;

		case 'M':
		case 'S':
			if (*buf == 0 ||
				isspace_l((unsigned char)*buf, locale))
				break;

			if (!isdigit_l((unsigned char)*buf, locale))
				return (NULL);

			len = 2;
			for (i = 0; len && *buf != 0 &&
				isdigit_l((unsigned char)*buf, locale); buf++){
				i *= 10;
				i += *buf - '0';
				len--;
			}

			if (c == 'M') {
				if (i > 59)
					return (NULL);
				tm->tm_min = i;
			} else {
				if (i > 60)
					return (NULL);
				tm->tm_sec = i;
			}

			break;

		case 'H':
		case 'I':
		case 'k':
		case 'l':
			/*
			 * Of these, %l is the only specifier explicitly
			 * documented as not being zero-padded.  However,
			 * there is no harm in allowing zero-padding.
			 *
			 * XXX The %l specifier may gobble one too many
			 * digits if used incorrectly.
			 */
			if (!isdigit_l((unsigned char)*buf, locale))
				return (NULL);

			len = 2;
			for (i = 0; len && *buf != 0 &&
			     isdigit_l((unsigned char)*buf, locale); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (c == 'H' || c == 'k') {
				if (i > 23)
					return (NULL);
			} else if (i > 12)
				return (NULL);

			tm->tm_hour = i;

			break;

		case 'p':
			/*
			 * XXX This is bogus if parsed before hour-related
			 * specifiers.
			 */
			len = strlen(tptr->am);
			if (strncasecmp_l(buf, tptr->am, len, locale) == 0) {
				if (tm->tm_hour > 12)
					return (NULL);
				if (tm->tm_hour == 12)
					tm->tm_hour = 0;
				buf += len;
				break;
			}

			len = strlen(tptr->pm);
			if (strncasecmp_l(buf, tptr->pm, len, locale) == 0) {
				if (tm->tm_hour > 12)
					return (NULL);
				if (tm->tm_hour != 12)
					tm->tm_hour += 12;
				buf += len;
				break;
			}

			return (NULL);

		case 'A':
		case 'a':
			for (i = 0; i < asizeof(tptr->weekday); i++) {
				len = strlen(tptr->weekday[i]);
				if (strncasecmp_l(buf, tptr->weekday[i],
						len, locale) == 0)
					break;
				len = strlen(tptr->wday[i]);
				if (strncasecmp_l(buf, tptr->wday[i],
						len, locale) == 0)
					break;
			}
			if (i == asizeof(tptr->weekday))
				return (NULL);

			buf += len;
			tm->tm_wday = i;
			flags |= FLAG_WDAY;
			break;

		case 'U':
		case 'W':
			/*
			 * XXX This is bogus, as we can not assume any valid
			 * information present in the tm structure at this
			 * point to calculate a real value, so just check the
			 * range for now.
			 */
			if (!isdigit_l((unsigned char)*buf, locale))
				return (NULL);

			len = 2;
			for (i = 0; len && *buf != 0 &&
			     isdigit_l((unsigned char)*buf, locale); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i > 53)
				return (NULL);

			if (c == 'U')
				day_offset = TM_SUNDAY;
			else
				day_offset = TM_MONDAY;


			week_offset = i;

			break;

		case 'w':
			if (!isdigit_l((unsigned char)*buf, locale))
				return (NULL);

			i = *buf - '0';
			if (i > 6)
				return (NULL);

			tm->tm_wday = i;
			flags |= FLAG_WDAY;

			break;

		case 'e':
			/*
			 * With %e format, our strftime(3) adds a blank space
			 * before single digits.
			 */
			if (*buf != 0 &&
			    isspace_l((unsigned char)*buf, locale))
			       buf++;
			/* FALLTHROUGH */
		case 'd':
			/*
			 * The %e specifier was once explicitly documented as
			 * not being zero-padded but was later changed to
			 * equivalent to %d.  There is no harm in allowing
			 * such padding.
			 *
			 * XXX The %e specifier may gobble one too many
			 * digits if used incorrectly.
			 */
			if (!isdigit_l((unsigned char)*buf, locale))
				return (NULL);

			len = 2;
			for (i = 0; len && *buf != 0 &&
			     isdigit_l((unsigned char)*buf, locale); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i > 31)
				return (NULL);

			tm->tm_mday = i;
			flags |= FLAG_MDAY;

			break;

		case 'B':
		case 'b':
		case 'h':
			for (i = 0; i < asizeof(tptr->month); i++) {
				if (Oalternative) {
					if (c == 'B') {
						len = strlen(tptr->alt_month[i]);
						if (strncasecmp_l(buf,
								tptr->alt_month[i],
								len, locale) == 0)
							break;
					}
				} else {
					len = strlen(tptr->month[i]);
					if (strncasecmp_l(buf, tptr->month[i],
							len, locale) == 0)
						break;
				}
			}
			/*
			 * Try the abbreviated month name if the full name
			 * wasn't found and Oalternative was not requested.
			 */
			if (i == asizeof(tptr->month) && !Oalternative) {
				for (i = 0; i < asizeof(tptr->month); i++) {
					len = strlen(tptr->mon[i]);
					if (strncasecmp_l(buf, tptr->mon[i],
							len, locale) == 0)
						break;
				}
			}
			if (i == asizeof(tptr->month))
				return (NULL);

			tm->tm_mon = i;
			buf += len;
			flags |= FLAG_MONTH;

			break;

		case 'm':
			if (!isdigit_l((unsigned char)*buf, locale))
				return (NULL);

			len = 2;
			for (i = 0; len && *buf != 0 &&
			     isdigit_l((unsigned char)*buf, locale); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (i < 1 || i > 12)
				return (NULL);

			tm->tm_mon = i - 1;
			flags |= FLAG_MONTH;

			break;

		case 'Y':
		case 'y':
			if (*buf == 0 ||
			    isspace_l((unsigned char)*buf, locale))
				break;

			if (!isdigit_l((unsigned char)*buf, locale))
				return (NULL);

			len = (c == 'Y') ? 4 : 2;
			for (i = 0; len && *buf != 0 &&
			     isdigit_l((unsigned char)*buf, locale); buf++) {
				i *= 10;
				i += *buf - '0';
				len--;
			}
			if (c == 'Y')
				i -= TM_YEAR_BASE;
			if (c == 'y' && i < 69)
				i += 100;
			if (i < 0)
				return (NULL);

			tm->tm_year = i;
			flags |= FLAG_YEAR;

			break;

		case 'n':
		case 't':
			while (isspace_l((unsigned char)*buf, locale))
				buf++;
			break;

		default:
			return (NULL);
		}
	}

	if (!(flags & FLAG_YDAY) && (flags & FLAG_YEAR)) {
		if ((flags & (FLAG_MONTH | FLAG_MDAY)) ==
		    (FLAG_MONTH | FLAG_MDAY)) {
			tm->tm_yday = start_of_month[isleap(tm->tm_year +
			    TM_YEAR_BASE)][tm->tm_mon] + (tm->tm_mday - 1);
			flags |= FLAG_YDAY;
		} else if (day_offset != -1) {
			/* Set the date to the first Sunday (or Monday)
			 * of the specified week of the year.
			 */
			if (!(flags & FLAG_WDAY)) {
				tm->tm_wday = day_offset;
				flags |= FLAG_WDAY;
			}
			tm->tm_yday = (7 -
			    first_wday_of(tm->tm_year + TM_YEAR_BASE) +
			    day_offset) % 7 + (week_offset - 1) * 7 +
			    tm->tm_wday - day_offset;
			flags |= FLAG_YDAY;
		}
	}

	if ((flags & (FLAG_YEAR | FLAG_YDAY)) == (FLAG_YEAR | FLAG_YDAY)) {
		if (!(flags & FLAG_MONTH)) {
			i = 0;
			while (tm->tm_yday >=
			    start_of_month[isleap(tm->tm_year +
			    TM_YEAR_BASE)][i])
				i++;
			if (i > 12) {
				i = 1;
				tm->tm_yday -=
				    start_of_month[isleap(tm->tm_year +
				    TM_YEAR_BASE)][12];
				tm->tm_year++;
			}
			tm->tm_mon = i - 1;
			flags |= FLAG_MONTH;
		}
		if (!(flags & FLAG_MDAY)) {
			tm->tm_mday = tm->tm_yday -
			    start_of_month[isleap(tm->tm_year + TM_YEAR_BASE)]
			    [tm->tm_mon] + 1;
			flags |= FLAG_MDAY;
		}
		if (!(flags & FLAG_WDAY)) {
			i = 0;
			wday_offset = first_wday_of(tm->tm_year);
			while (i++ <= tm->tm_yday) {
				if (wday_offset++ >= 6)
					wday_offset = 0;
			}
			tm->tm_wday = wday_offset;
			flags |= FLAG_WDAY;
		}
	}

	return ((char *)buf);
}

static char *
strptime_l(const char * __restrict buf, const char * __restrict fmt,
    struct tm * __restrict tm, locale_t loc)
{
	char *ret;
	int gmt;
	FIX_LOCALE(loc);

	gmt = 0;
	ret = _strptime(buf, fmt, tm, &gmt, loc);

	return (ret);
}

static char *
strptime(const char * __restrict buf, const char * __restrict fmt,
    struct tm * __restrict tm)
{
	return strptime_l(buf, fmt, tm, __get_locale());
}
