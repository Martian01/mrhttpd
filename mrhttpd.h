/*

mrhttpd v2.4.1
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


#ifndef MRHTTPD_INCLUDE
#define MRHTTPD_INCLUDE

#define DEBUG 0

#define _REENTRANT

#include <fcntl.h>
#include <pthread.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>

#include "config.h"

#define SERVER_SOFTWARE   "mrhttpd/2.4.1"

#define PROTOCOL_HTTP_1_0 "HTTP/1.0"
#define PROTOCOL_HTTP_1_1 "HTTP/1.1"

#define ESTRANGE 1024

enum error_state { ERROR_FALSE, ERROR_TRUE };

enum connection_state { CONNECTION_KEEPALIVE, CONNECTION_CLOSE };

typedef struct {
	int size;
	int current;
	char *mem;
} memdescr;

typedef struct {
	int max;
	int current;
	char **index;
	memdescr *md;
} indexdescr;

// main
int main(void);
void *serverthread(void *);
void sigterm_handler(const int);
void sigchld_handler(const int);

// protocol
enum connection_state http_request(const int);

// io
void set_timeout(const int);
int recv_header_with_timeout(const int , indexdescr *);
ssize_t send_with_timeout(const int, const char *, const ssize_t);
ssize_t mysendfile(const int, const int, ssize_t);
ssize_t sendfile_with_timeout(const int, const int, const ssize_t);

// mem
void md_init(memdescr *);
enum error_state md_add(memdescr *, const char *);
enum error_state md_extend(memdescr *, const char *);
enum error_state md_extend_char(memdescr *, const char);
enum error_state md_extend_number(memdescr *, const unsigned);
enum error_state md_translate(memdescr *, const char, const char);
void id_init(indexdescr *);
enum error_state id_add_string(indexdescr *, const char *);
enum error_state id_add_env_string(indexdescr *, const char *, const char *);
enum error_state id_add_env_number(indexdescr *, const char *, const unsigned);
enum error_state id_add_env_http_variables(indexdescr *, indexdescr *);
char *id_read_variable(indexdescr *, const char *);

// util
extern FILE *logfile;
int Log_open(const int);
void Log_close(const int);
void Log(const int, const char *, ...);
const char *mimetype(const char *);
int hexdigit(const char);
enum error_state url_decode(const char *, char *, size_t);
enum error_state filename_encode(const char *, char *, size_t);
char *str_tolower(char *);
char *str_toupper(char *);
char *startof(char *);

#endif
