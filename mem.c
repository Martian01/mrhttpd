/*

mrhttpd v2.4.3
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
// This may appear rigid, but it guarantees the absence of memory leaks.

// In a way, mempool and stringpool are class definitions (in mrhttpd.h),
// and the functions in this include are their instance methods.

void mp_reset(mempool *mp) {
	mp->current = 0;
}

enum error_state mp_add(mempool *mp, const char *string) {
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

enum error_state mp_extend(mempool *mp, const char *string) {
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

enum error_state mp_extend_char(mempool *mp, const char c) {
	int target;

	target = (mp->current == 0) ? 0 : (mp->current - 1);
	if ((target + 2) >= mp->size)
		return ERROR_TRUE; // memory allocation failed
	mp->mem[target++] = c;
	mp->mem[target++] = '\0';
	mp->current = target;
	return ERROR_FALSE; // success
}

enum error_state mp_extend_number(mempool *mp, const unsigned num) {
	static char digit[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
	
	if (num < 10)
		return mp_extend_char(mp, digit[num]);
		
	return mp_extend_number(mp, num / 10 ) || mp_extend_char(mp, digit[num % 10]);

}

void mp_replace(mempool *mp, const char from, const char to) {
	char *cp;
	int i;
	
	for (cp = mp->mem, i = mp->current; i > 0; cp++, i--)
		if (*cp == from)
		  *cp = to;
}

void sp_reset(stringpool *sp) {
	mp_reset(sp->mp);
	sp->current = 0;
}

enum error_state sp_add(stringpool *sp, const char *string) {
	int target;

	if (string == NULL)
		return ERROR_TRUE; // deferred detection of error condition

	if (sp->current >= sp->size)
		return ERROR_TRUE; // no space left in index array

	target = sp->mp->current;
	if (mp_add(sp->mp, string))
		return ERROR_TRUE; // memory allocation failed

	sp->strings[sp->current++] = sp->mp->mem + target;
	return ERROR_FALSE; // success
}

enum error_state sp_add_variable(stringpool *sp, const char *varname, const char *value) {
	return
		sp_add(sp, varname) ||
		mp_extend(sp->mp, "=") ||
		mp_extend(sp->mp, value)
		;
}

enum error_state sp_add_variable_number(stringpool *sp, const char *varname, const unsigned value) {
	return
		sp_add(sp, varname) ||
		mp_extend(sp->mp, "=") ||
		mp_extend_number(sp->mp, value)
		;
}

enum error_state sp_add_variables(stringpool *sp, const stringpool *var, const char *prefix) {
	char **strp;
	char *varname, *value;
	int i;
	
	for (strp = var->strings, i = var->current; i > 0; strp++, i--) {
		value = *strp;
		varname = strsep(&value, ":");
		if (value != NULL)
			if (
					sp_add(sp, prefix) ||
					mp_extend(sp->mp, str_toupper(varname)) ||
					mp_extend(sp->mp, "=") ||
					mp_extend(sp->mp, startof(value))
				)
				return ERROR_TRUE; // otherwise continue
	}
	return ERROR_FALSE;

}

char *sp_read_variable(const stringpool *sp, const char *varname) {
	size_t namelength;
	char **strp;
	char *value;
	int i;
	
	namelength = strlen(varname);
	for (strp = sp->strings, i = sp->current; i > 0; strp++, i--) {
		if (strncmp(*strp, varname, namelength) == 0) {
			value = *strp + namelength;
			if (*value == ':')
				return startof(++value);
		}
	}
	return NULL; // variable not found
}

