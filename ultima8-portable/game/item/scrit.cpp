// flags: -P -3 -O -Ob -Oe -Og -Oi -Ol -Om -Ov -Z -G -k- -d -r-
// name: ..\ITEM\SCRIT.C

#include "u8port.h"

#include <stdarg.h>
// #include <mem.h>  (DOS only)
#include "error.h"
#include "scrit.h"

SearchCriteria::SearchCriteria(int16_t command, ...)
{
	int16_t pos = 1;
	int16_t c;
	int16_t value;
	int16_t i;
	int16_t len;
	va_list ap;

	va_start(ap, command);
	for (c = command; c != '$'; c = (int16_t)va_arg(ap, int))
	{
		len = cmdlen(c);
		if (len + pos < 31)
		{
			data[pos++] = c;
			for (i = 1; i < len; i += 2)
			{
				value = (int16_t)va_arg(ap, int);
				data[pos++] = value & 0xff;
				data[pos++] = value >> 8;
			}
		}
		else
			halt(__FILE__, 66);	// __LINE__
	}
	data[pos] = '$';
	data[0] = pos;
}

int16_t SearchCriteria::cmdlen(int16_t c)
{
	if (c == '%')
		return 3;
	if (c > '@' && c < '`')
		return (c - '@') * 2 + 1;
	if (c > '`' && c < 0x80)
		return (c - '`') * 2 + 1;
	return 1;
}

void SearchCriteria::operator+=(SearchCriteria other)
{
	int16_t len = data[0];
	int16_t otherLen = other.data[0];
	data[0] = len + otherLen - 1;
	memcpy(&data[len], &other.data[1], otherLen);
}
