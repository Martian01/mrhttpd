/*

mrhttpd v2.8.0
Copyright (c) 2007-2021  Martin Rogge <martin_rogge@users.sourceforge.net>

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

void memPoolReset(MemPool* mp) {
	mp->current = 0;
}

void memPoolResetTo(MemPool* mp, int savePosition) {
	mp->current = savePosition;
	if (savePosition > 0)
		mp->mem[savePosition - 1] = '\0';
}

int memPoolNextTarget(MemPool* mp) {
	return mp->current == 0 ? 0 : mp->current - 1;
}

boolean memPoolAdd(MemPool* mp, const char* string) {
	int added;

	if (string == null)
		return true; // deferred detection of error condition

	added = strlen(string) + 1;

	if ((mp->current + added) >= mp->size)
		return true; // memory allocation failed
	memcpy(mp->mem + mp->current, string, added);
	mp->current += added;
	return false; // success
}

boolean memPoolExtend(MemPool* mp, const char* string) {
	int target, added;

	if (string == null)
		return true; // deferred detection of error condition

	target = memPoolNextTarget(mp);
	added = strlen(string) + 1;

	if ((target + added) >= mp->size)
		return true; // memory allocation failed
	memcpy(mp->mem + target, string, added);
	mp->current = target + added;
	return false; // success
}

boolean memPoolExtendChar(MemPool* mp, const char c) {
	int target;

	target = memPoolNextTarget(mp);
	if ((target + 2) >= mp->size)
		return true; // memory allocation failed
	mp->mem[target++] = c;
	mp->mem[target++] = '\0';
	mp->current = target;
	return false; // success
}

boolean memPoolExtendNumber(MemPool* mp, const unsigned num) {
	return num < 10 ? memPoolExtendChar(mp, digit[num]) : (memPoolExtendNumber(mp, num / 10 ) || memPoolExtendChar(mp, digit[num % 10]));
}

void memPoolReplace(MemPool* mp, const char from, const char to) {
	char* cp;
	int i;
	
	for (cp = mp->mem, i = mp->current; i > 0; cp++, i--)
		if (*cp == from)
		  *cp = to;
}

int memPoolLineBreak(const MemPool* mp, const int start) {
	int i;
	
	for (i = start; i < mp->current - 1; i++)
		if (mp->mem[i] == '\r' && mp->mem[i + 1] == '\n')
			return i;
	return -1;
}

void stringPoolReset(StringPool* sp) {
	memPoolReset(sp->mp);
	sp->current = 0;
}

boolean stringPoolAdd(StringPool* sp, const char* string) {
	int target;

	if (string == null)
		return true; // deferred detection of error condition

	if (sp->current >= sp->size)
		return true; // no space left in index array

	target = sp->mp->current;
	if (memPoolAdd(sp->mp, string))
		return true; // memory allocation failed

	sp->strings[sp->current++] = sp->mp->mem + target;
	return false; // success
}

boolean stringPoolAddVariable(StringPool* sp, const char* varName, const char* value) {
	return
		stringPoolAdd(sp, varName) ||
		memPoolExtend(sp->mp, "=") ||
		memPoolExtend(sp->mp, value)
		;
}

boolean stringPoolAddVariableNumber(StringPool* sp, const char* varName, const unsigned value) {
	return
		stringPoolAdd(sp, varName) ||
		memPoolExtend(sp->mp, "=") ||
		memPoolExtendNumber(sp->mp, value)
		;
}

boolean stringPoolAddVariables(StringPool* sp, const StringPool* var, const char* prefix) {
	char** strp;
	char* varName, *value;
	int i;
	
	for (strp = var->strings, i = var->current; i > 0; strp++, i--) {
		value = *strp;
		varName = strsep(&value, ":");
		if (value != null)
			if (
					stringPoolAdd(sp, prefix) ||
					memPoolExtend(sp->mp, strToUpper(varName)) ||
					memPoolExtend(sp->mp, "=") ||
					memPoolExtend(sp->mp, startOf(value))
				)
				return true; // otherwise continue
	}
	return false;

}

char* stringPoolReadHttpHeader(const StringPool* sp, const char* headerName) {
	char** strp;
	int i;
	
	for (strp = sp->strings, i = sp->current; i > 0; strp++, i--) {
		char* value = removePrefix(headerName, *strp);
		if (value != null && *value == ':')
				return startOf(++value);
	}
	return null; // variable not found
}

char* removePrefix(const char* prefix, char* string) {
	while (*prefix) {
		if (*prefix++ != tolower(*string++)) // HTTP header names are case-insensitive according to RFC 2616
			return null; // string does not start with prefix
	}
	return string; // remainder
}
