#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "asm/gnustring.h"
#include "asm/types.h"
#include "asm/main.h"

char* strdup (const char* str)
{
	char* retstr = NULL;
	int sz = strlen(str) + 1;
	if ((retstr = (char*)malloc(sz)))
	{
		strcpy(retstr, str);
	}
	else
	{
		fatalerror("Could not allocate memory for string.");
	}
	return retstr;
}

int	strncasecmp(const char* first, const char* second, int count)
{
	#define lower(a) (tolower((unsigned char)*(a)))
	if (count == 0)
	{
		return 0;
	}
	while (count-- != 0 && lower(first) == lower(second))
	{
		if (count == 0 || *first == '\0' || *second == '\0')
		{
			break;
		}
		first++;
		second++;
	}
	return lower(first) - lower(second);
	#undef lower
}