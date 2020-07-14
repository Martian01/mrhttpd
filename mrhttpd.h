/*

mrhttpd v2.5.0
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

#define SERVER_SOFTWARE   "mrhttpd/2.5.0"

#define PROTOCOL_HTTP_1_0 "HTTP/1.0"
#define PROTOCOL_HTTP_1_1 "HTTP/1.1"

#define ESTRANGE 1024

enum errorState { ERROR_FALSE, ERROR_TRUE };

enum connectionState { CONNECTION_KEEPALIVE, CONNECTION_CLOSE };

typedef struct {
	int size;
	int current;
	char *mem;
} memPool;

typedef struct {
	int size;
	int current;
	char **strings;
	memPool *mp;
} stringPool;

// main
int main(void);
void *serverThread(void *);
void sigtermHandler(const int);
void sigchldHandler(const int);

// protocol
enum connectionState httpRequest(const int);

// io
void setTimeout(const int);
int receiveHeader(const int, stringPool *, memPool *);
ssize_t sendMemPool(const int, const memPool *);
ssize_t sendBuffer(const int, const char *, const ssize_t);
ssize_t sendFile(const int, const int, const ssize_t);
ssize_t receiveFile(const int, const int, const ssize_t);
ssize_t pipeStream(const int, const int, ssize_t);

// mem
void memPoolReset(memPool *);
enum errorState memPoolAdd(memPool *, const char *);
enum errorState memPoolExtend(memPool *, const char *);
enum errorState memPoolExtendChar(memPool *, const char);
enum errorState memPoolExtendNumber(memPool *, const unsigned);
void memPoolReplace(memPool *, const char, const char);
int memPoolLineBreak(const memPool *, const int);

void stringPoolReset(stringPool *);
enum errorState stringPoolAdd(stringPool *, const char *);
enum errorState stringPoolAddVariable(stringPool *, const char *, const char *);
enum errorState stringPoolAddVariableNumber(stringPool *, const char *, const unsigned);
enum errorState stringPoolAddVariables(stringPool *, const stringPool *, const char*);
char *stringPoolReadVariable(const stringPool *, const char *);

// util
extern FILE *logFile;
int LogOpen(const int);
void LogClose(const int);
void Log(const int, const char *, ...);
const char *mimeType(const char *);
int hexDigit(const char);
enum errorState urlDecode(const char *, char *, size_t);
enum errorState fileNameEncode(const char *, char *, size_t);
char *strToLower(char *);
char *strToUpper(char *);
char *startOf(char *);

#endif
