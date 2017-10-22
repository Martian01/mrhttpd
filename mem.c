/*

MrHTTPD v2.2.0
Copyright (c) 2007-2011  Martin Rogge <martin_rogge@users.sourceforge.net>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

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

enum error_state md_init(memdescr *md, const char *str1)
{
	int len1;

	if (str1 == NULL)
		return ERROR_TRUE; // deferred detection of undesirable condition

	len1 = strlen(str1);
	if (len1 >= md->size)
		return ERROR_TRUE; // memory allocation failed
	memcpy(md->mem, str1, len1);
	md->mem[len1] = '\0';
	md->current = len1 + 1;
	return ERROR_FALSE; // success
}

enum error_state md_extend(memdescr *md, const char *str2)
{
	int len1, len2;

	if (str2 == NULL)
		return ERROR_TRUE; // deferred detection of undesirable condition

	len1 = (md->current == 0)?0:(md->current - 1);
	len2 = strlen(str2);
	if ((len1 + len2) >= md->size)
		return ERROR_TRUE; // memory allocation failed
	memcpy(md->mem + len1, str2, len2);
	md->mem[len1 + len2] = '\0';
	md->current = len1 + len2 + 1;
	return ERROR_FALSE; // success
}

enum error_state md_extend_char(memdescr *md, const char c)
{
	int len;

	len = (md->current == 0)?0:(md->current - 1);
	if ((len + 1) >= md->size)
		return ERROR_TRUE; // memory allocation failed
	md->mem[len++] = c;
	md->mem[len++] = '\0';
	md->current = len;
	return ERROR_FALSE; // success
}

enum error_state md_extend_number(memdescr *md, const unsigned num)
{
	static char digit[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
	
	if (num < 10)
		return md_extend_char(md, digit[num]);
		
	return md_extend_number(md, num / 10 ) || md_extend_char(md, digit[num % 10]);

}

enum error_state md_translate(memdescr *md, const char from, const char to)
{
	char *cp;
	int i;
	
	for (cp = md->mem, i = md->current; i > 0; cp++, i--)
		if (*cp == from)
		  *cp = to;
	return ERROR_FALSE; // success
}

enum error_state id_add_string(indexdescr *id, const char *str1)
{
	int len1;
	char *str;
	memdescr *md;

	if (str1 == NULL)
		return ERROR_TRUE; // deferred detection of undesirable condition ;)

	if (id->current >= id->max)
		return ERROR_TRUE; // no space left in index array

	md = id->md;
	len1 = strlen(str1);
	if ((md->current + len1) >= md->size)
		return ERROR_TRUE; // memory allocation failed
	str = md->mem + md->current;
	md->current += len1 + 1;
	memcpy(str, str1, len1);
	str[len1] = '\0';
	id->index[id->current++] = str;
	id->index[id->current] = NULL;
	return ERROR_FALSE; // success
}

enum error_state id_add_env_string(indexdescr *eid, const char *variable, const char *value)
{
	return
		id_add_string(eid, variable) ||
		md_extend(eid->md, "=") ||
		md_extend(eid->md, value)
		;
}

enum error_state id_add_env_number(indexdescr *eid, const char *variable, const unsigned num)
{
	return
		id_add_string(eid, variable) ||
		md_extend(eid->md, "=") ||
		md_extend_number(eid->md, num)
		;
}

enum error_state id_add_env_http_variables(indexdescr *eid, indexdescr *hid)
{
	char **strp;
	char *name, *value;
	
	for (strp = hid->index; *strp != NULL; strp++) {
		value = *strp;
		name = strsep(&value, ":");
		if (value != NULL)
			if (
					id_add_string(eid, "HTTP_") ||
					md_extend(eid->md, str_toupper(name)) ||
					md_extend(eid->md, "=") ||
					md_extend(eid->md, startof(value))
				)
				return ERROR_TRUE; // otherwise continue
	}
	return ERROR_FALSE;

}

char *id_read_variable(indexdescr *id, const char *variable)
{
	size_t vlen;
	char **strp;
	char *value;
	
	vlen = strlen(variable);
	for (strp = id->index; *strp != NULL; strp++) {
		if (strncmp(*strp, variable, vlen) == 0) {
			value = *strp+vlen;
			if (*value == ':') {
				return startof(++value);
			}
		}
	}
	return NULL; // variable not found
}

