/*

mrhttpd v2.4.4
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

#define SERVER_SOFTWARE   "mrhttpd/2.4.4"

#define PROTOCOL_HTTP_1_0 "HTTP/1.0"
#define PROTOCOL_HTTP_1_1 "HTTP/1.1"

#define ESTRANGE 1024

enum error_state { ERROR_FALSE, ERROR_TRUE };

enum connection_state { CONNECTION_KEEPALIVE, CONNECTION_CLOSE };

typedef struct {
	int size;
	int current;
	char *mem;
} mempool;

typedef struct {
	int size;
	int current;
	char **strings;
	mempool *mp;
} stringpool;

// main
int main(void);
void *serverthread(void *);
void sigterm_handler(const int);
void sigchld_handler(const int);

// protocol
enum connection_state http_request(const int);

// io
void set_timeout(const int);
int recv_header_with_timeout(stringpool *, const int);
ssize_t send_with_timeout(const int, const char *, const ssize_t);
ssize_t mysendfile(const int, const int, ssize_t);
ssize_t sendfile_with_timeout(const int, const int, const ssize_t);

// mem
void mp_reset(mempool *);
enum error_state mp_add(mempool *, const char *);
enum error_state mp_extend(mempool *, const char *);
enum error_state mp_extend_char(mempool *, const char);
enum error_state mp_extend_number(mempool *, const unsigned);
void mp_replace(mempool *, const char, const char);
void sp_reset(stringpool *);
enum error_state sp_add(stringpool *, const char *);
enum error_state sp_add_variable(stringpool *, const char *, const char *);
enum error_state sp_add_variable_number(stringpool *, const char *, const unsigned);
enum error_state sp_add_variables(stringpool *, const stringpool *, const char*);
char *sp_read_variable(const stringpool *, const char *);

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
