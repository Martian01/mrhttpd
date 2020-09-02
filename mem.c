/*

mrhttpd v2.5.4
Copyright (c) 2007-2020  Martin Rogge <martin_rogge@users.sourceforge.net>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "mrhttpd.h"

// Memory management functions

// Containing some trickery with local memory pools to avoid the management of
// global heap memory using malloc() and free().

// The concept of fixed memory arrays backing variable-length strings and string arrays
// is not entirely dissimilar to the concept of slices in Go.

// This may appear rigid, but it guarantees the absence of memory leaks.

// In a way, MemPool and StringPool are class definitions (in mrhttpd.h),
// and the functions in this include are their instance methods.

void memPoolReset(MemPool *mp) {
	mp->current = 0;
}

void memPoolResetTo(MemPool *mp, int savePosition) {
	mp->current = savePosition;
	if (savePosition > 0)
		mp->mem[savePosition - 1] = '\0';
}

enum ErrorState memPoolAdd(MemPool *mp, const char *string) {
	int added;

	if (string == NULL)
		return ERROR_TRUE; // deferred detection of error condition

	added = strlen(string) + 1;

	if ((mp->current + added) >= mp->size)
		return ERROR_TRUE; // memory allocation failed
	memcpy(mp->mem + mp->current, string, added);
	mp->current += added;
	return ERROR_FALSE; // success
}

enum ErrorState memPoolExtend(MemPool *mp, const char *string) {
	int target, added;

	if (string == NULL)
		return ERROR_TRUE; // deferred detection of error condition

	target = (mp->current == 0) ? 0 : (mp->current - 1);
	added = strlen(string) + 1;

	if ((target + added) >= mp->size)
		return ERROR_TRUE; // memory allocation failed
	memcpy(mp->mem + target, string, added);
	mp->current = target + added;
	return ERROR_FALSE; // success
}

enum ErrorState memPoolExtendChar(MemPool *mp, const char c) {
	int target;

	target = (mp->current == 0) ? 0 : (mp->current - 1);
	if ((target + 2) >= mp->size)
		return ERROR_TRUE; // memory allocation failed
	mp->mem[target++] = c;
	mp->mem[target++] = '\0';
	mp->current = target;
	return ERROR_FALSE; // success
}

enum ErrorState memPoolExtendNumber(MemPool *mp, const unsigned num) {
	return num < 10 ? memPoolExtendChar(mp, digit[num]) : (memPoolExtendNumber(mp, num / 10 ) || memPoolExtendChar(mp, digit[num % 10]));
}

void memPoolReplace(MemPool *mp, const char from, const char to) {
	char *cp;
	int i;
	
	for (cp = mp->mem, i = mp->current; i > 0; cp++, i--)
		if (*cp == from)
		  *cp = to;
}

int memPoolLineBreak(const MemPool *mp, const int start) {
	int i;
	
	for (i = start; i < mp->current - 1; i++)
		if (mp->mem[i] == '\r' && mp->mem[i + 1] == '\n')
			return i;
	return -1;
}

void stringPoolReset(StringPool *sp) {
	memPoolReset(sp->mp);
	sp->current = 0;
}

enum ErrorState stringPoolAdd(StringPool *sp, const char *string) {
	int target;

	if (string == NULL)
		return ERROR_TRUE; // deferred detection of error condition

	if (sp->current >= sp->size)
		return ERROR_TRUE; // no space left in index array

	target = sp->mp->current;
	if (memPoolAdd(sp->mp, string))
		return ERROR_TRUE; // memory allocation failed

	sp->strings[sp->current++] = sp->mp->mem + target;
	return ERROR_FALSE; // success
}

enum ErrorState stringPoolAddVariable(StringPool *sp, const char *varName, const char *value) {
	return
		stringPoolAdd(sp, varName) ||
		memPoolExtend(sp->mp, "=") ||
		memPoolExtend(sp->mp, value)
		;
}

enum ErrorState stringPoolAddVariableNumber(StringPool *sp, const char *varName, const unsigned value) {
	return
		stringPoolAdd(sp, varName) ||
		memPoolExtend(sp->mp, "=") ||
		memPoolExtendNumber(sp->mp, value)
		;
}

enum ErrorState stringPoolAddVariables(StringPool *sp, const StringPool *var, const char *prefix) {
	char **strp;
	char *varName, *value;
	int i;
	
	for (strp = var->strings, i = var->current; i > 0; strp++, i--) {
		value = *strp;
		varName = strsep(&value, ":");
		if (value != NULL)
			if (
					stringPoolAdd(sp, prefix) ||
					memPoolExtend(sp->mp, strToUpper(varName)) ||
					memPoolExtend(sp->mp, "=") ||
					memPoolExtend(sp->mp, startOf(value))
				)
				return ERROR_TRUE; // otherwise continue
	}
	return ERROR_FALSE;

}

char *stringPoolReadVariable(const StringPool *sp, const char *varName) {
	size_t nameLength;
	char **strp;
	char *value;
	int i;
	
	nameLength = strlen(varName);
	for (strp = sp->strings, i = sp->current; i > 0; strp++, i--) {
		if (strncmp(*strp, varName, nameLength) == 0) {
			value = *strp + nameLength;
			if (*value == ':')
				return startOf(++value);
		}
	}
	return NULL; // variable not found
}
