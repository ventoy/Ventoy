/*
	time.c (03.02.12)
	exFAT file system implementation library.

	Free exFAT implementation.
	Copyright (C) 2010-2018  Andrew Nayenko

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "exfat.h"

/* timezone offset from UTC in seconds; positive for western timezones,
   negative for eastern ones */
static long exfat_timezone;

#define SEC_IN_MIN 60ll
#define SEC_IN_HOUR (60 * SEC_IN_MIN)
#define SEC_IN_DAY (24 * SEC_IN_HOUR)
#define SEC_IN_YEAR (365 * SEC_IN_DAY) /* not leap year */
/* Unix epoch started at 0:00:00 UTC 1 January 1970 */
#define UNIX_EPOCH_YEAR 1970
/* exFAT epoch started at 0:00:00 UTC 1 January 1980 */
#define EXFAT_EPOCH_YEAR 1980
/* number of years from Unix epoch to exFAT epoch */
#define EPOCH_DIFF_YEAR (EXFAT_EPOCH_YEAR - UNIX_EPOCH_YEAR)
/* number of days from Unix epoch to exFAT epoch (considering leap days) */
#define EPOCH_DIFF_DAYS (EPOCH_DIFF_YEAR * 365 + EPOCH_DIFF_YEAR / 4)
/* number of seconds from Unix epoch to exFAT epoch (considering leap days) */
#define EPOCH_DIFF_SEC (EPOCH_DIFF_DAYS * SEC_IN_DAY)
/* number of leap years passed from exFAT epoch to the specified year
   (excluding the specified year itself) */
#define LEAP_YEARS(year) ((EXFAT_EPOCH_YEAR + (year) - 1) / 4 \
		- (EXFAT_EPOCH_YEAR - 1) / 4)
/* checks whether the specified year is leap */
#define IS_LEAP_YEAR(year) ((EXFAT_EPOCH_YEAR + (year)) % 4 == 0)

static const time_t days_in_year[] =
{
	/* Jan  Feb  Mar  Apr  May  Jun  Jul  Aug  Sep  Oct  Nov  Dec */
	0,   0,  31,  59,  90, 120, 151, 181, 212, 243, 273, 304, 334
};

time_t exfat_exfat2unix(le16_t date, le16_t time, uint8_t centisec)
{
	time_t unix_time = EPOCH_DIFF_SEC;
	uint16_t ndate = le16_to_cpu(date);
	uint16_t ntime = le16_to_cpu(time);

	uint16_t day    = ndate & 0x1f;      /* 5 bits, 1-31 */
	uint16_t month  = ndate >> 5 & 0xf;  /* 4 bits, 1-12 */
	uint16_t year   = ndate >> 9;        /* 7 bits, 1-127 (+1980) */

	uint16_t twosec = ntime & 0x1f;      /* 5 bits, 0-29 (2 sec granularity) */
	uint16_t min    = ntime >> 5 & 0x3f; /* 6 bits, 0-59 */
	uint16_t hour   = ntime >> 11;       /* 5 bits, 0-23 */

	if (day == 0 || month == 0 || month > 12)
	{
		exfat_error("bad date %u-%02hu-%02hu",
				year + EXFAT_EPOCH_YEAR, month, day);
		return 0;
	}
	if (hour > 23 || min > 59 || twosec > 29)
	{
		exfat_error("bad time %hu:%02hu:%02u",
				hour, min, twosec * 2);
		return 0;
	}
	if (centisec > 199)
	{
		exfat_error("bad centiseconds count %hhu", centisec);
		return 0;
	}

	/* every 4th year between 1904 and 2096 is leap */
	unix_time += year * SEC_IN_YEAR + LEAP_YEARS(year) * SEC_IN_DAY;
	unix_time += days_in_year[month] * SEC_IN_DAY;
	/* if it's leap year and February has passed we should add 1 day */
	if ((EXFAT_EPOCH_YEAR + year) % 4 == 0 && month > 2)
		unix_time += SEC_IN_DAY;
	unix_time += (day - 1) * SEC_IN_DAY;

	unix_time += hour * SEC_IN_HOUR;
	unix_time += min * SEC_IN_MIN;
	/* exFAT represents time with 2 sec granularity */
	unix_time += twosec * 2;
	unix_time += centisec / 100;

	/* exFAT stores timestamps in local time, so we correct it to UTC */
	unix_time += exfat_timezone;

	return unix_time;
}

void exfat_unix2exfat(time_t unix_time, le16_t* date, le16_t* time,
		uint8_t* centisec)
{
	time_t shift = EPOCH_DIFF_SEC + exfat_timezone;
	uint16_t day, month, year;
	uint16_t twosec, min, hour;
	int days;
	int i;

	/* time before exFAT epoch cannot be represented */
	if (unix_time < shift)
		unix_time = shift;

	unix_time -= shift;

	days = unix_time / SEC_IN_DAY;
	year = (4 * days) / (4 * 365 + 1);
	days -= year * 365 + LEAP_YEARS(year);
	month = 0;
	for (i = 1; i <= 12; i++)
	{
		int leap_day = (IS_LEAP_YEAR(year) && i == 2);
		int leap_sub = (IS_LEAP_YEAR(year) && i >= 3);

		if (i == 12 || days - leap_sub < days_in_year[i + 1] + leap_day)
		{
			month = i;
			days -= days_in_year[i] + leap_sub;
			break;
		}
	}
	day = days + 1;

	hour = (unix_time % SEC_IN_DAY) / SEC_IN_HOUR;
	min = (unix_time % SEC_IN_HOUR) / SEC_IN_MIN;
	twosec = (unix_time % SEC_IN_MIN) / 2;

	*date = cpu_to_le16(day | (month << 5) | (year << 9));
	*time = cpu_to_le16(twosec | (min << 5) | (hour << 11));
	if (centisec)
		*centisec = (unix_time % 2) * 100;
}

void exfat_tzset(void)
{
	time_t now;
	struct tm* utc;

	tzset();
	now = time(NULL);
	utc = gmtime(&now);
	/* gmtime() always sets tm_isdst to 0 because daylight savings never
	   affect UTC. Setting tm_isdst to -1 makes mktime() to determine whether
	   summer time is in effect. */
	utc->tm_isdst = -1;
	exfat_timezone = mktime(utc) - now;
}
