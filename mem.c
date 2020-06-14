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

// In a way, memdescr and indexdescr are class definitions (in mrhttpd.h),
// and the functions in this include are their instance methods.

void md_reset(memdescr *md) {
	md->current = 0;
}

enum error_state md_add(memdescr *md, const char *string) {
	int added;

	if (string == NULL)
		return ERROR_TRUE; // deferred detection of error condition

	added = strlen(string) + 1;

	if ((md->current + added) >= md->size)
		return ERROR_TRUE; // memory allocation failed
	memcpy(md->mem + md->current, string, added);
	md->current += added;
	return ERROR_FALSE; // success
}

enum error_state md_extend(memdescr *md, const char *string) {
	int target, added;

	if (string == NULL)
		return ERROR_TRUE; // deferred detection of error condition

	target = (md->current == 0) ? 0 : (md->current - 1);
	added = strlen(string) + 1;

	if ((target + added) >= md->size)
		return ERROR_TRUE; // memory allocation failed
	memcpy(md->mem + target, string, added);
	md->current = target + added;
	return ERROR_FALSE; // success
}

enum error_state md_extend_char(memdescr *md, const char c) {
	int target;

	target = (md->current == 0) ? 0 : (md->current - 1);
	if ((target + 2) >= md->size)
		return ERROR_TRUE; // memory allocation failed
	md->mem[target++] = c;
	md->mem[target++] = '\0';
	md->current = target;
	return ERROR_FALSE; // success
}

enum error_state md_extend_number(memdescr *md, const unsigned num) {
	static char digit[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
	
	if (num < 10)
		return md_extend_char(md, digit[num]);
		
	return md_extend_number(md, num / 10 ) || md_extend_char(md, digit[num % 10]);

}

enum error_state md_translate(memdescr *md, const char from, const char to) {
	char *cp;
	int i;
	
	for (cp = md->mem, i = md->current; i > 0; cp++, i--)
		if (*cp == from)
		  *cp = to;
	return ERROR_FALSE; // success
}

void id_reset(indexdescr *id) {
	md_reset(id->md);
	id->current = 0;
}

enum error_state id_add(indexdescr *id, const char *string) {
	int target;

	if (string == NULL)
		return ERROR_TRUE; // deferred detection of error condition

	if (id->current >= id->max)
		return ERROR_TRUE; // no space left in index array

	target = id->md->current;
	if (md_add(id->md, string))
		return ERROR_TRUE; // memory allocation failed

	id->index[id->current++] = id->md->mem + target;
	return ERROR_FALSE; // success
}

enum error_state id_add_variable(indexdescr *id, const char *varname, const char *value) {
	return
		id_add(id, varname) ||
		md_extend(id->md, "=") ||
		md_extend(id->md, value)
		;
}

enum error_state id_add_variable_number(indexdescr *id, const char *varname, const unsigned value) {
	return
		id_add(id, varname) ||
		md_extend(id->md, "=") ||
		md_extend_number(id->md, value)
		;
}

enum error_state id_add_variables(indexdescr *id, const indexdescr *var, const char *prefix) {
	char **strp;
	char *varname, *value;
	int i;
	
	for (strp = var->index, i = var->current; i > 0; strp++, i--) {
		value = *strp;
		varname = strsep(&value, ":");
		if (value != NULL)
			if (
					id_add(id, prefix) ||
					md_extend(id->md, str_toupper(varname)) ||
					md_extend(id->md, "=") ||
					md_extend(id->md, startof(value))
				)
				return ERROR_TRUE; // otherwise continue
	}
	return ERROR_FALSE;

}

char *id_read_variable(const indexdescr *id, const char *varname) {
	size_t namelength;
	char **strp;
	char *value;
	int i;
	
	namelength = strlen(varname);
	for (strp = id->index, i = id->current; i > 0; strp++, i--) {
		if (strncmp(*strp, varname, namelength) == 0) {
			value = *strp + namelength;
			if (*value == ':')
				return startof(++value);
		}
	}
	return NULL; // variable not found
}

