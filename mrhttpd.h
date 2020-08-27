/*

mrhttpd v2.5.3
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

#include "config.h"

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

#ifdef EXT_FILE_CMD
#include <unistd.h>
#endif

#if AUTO_INDEX == 1
#include <dirent.h>
#endif

#if USE_SENDFILE == 1
#include <sys/sendfile.h>
#endif

#define SERVER_NAME       "mrhttpd"
#define SERVER_SOFTWARE   "mrhttpd/2.5.3"

#define PROTOCOL_HTTP_1_0 "HTTP/1.0"
#define PROTOCOL_HTTP_1_1 "HTTP/1.1"

#define ESTRANGE 1024

enum ErrorState { ERROR_FALSE, ERROR_TRUE };

enum ConnectionState { CONNECTION_KEEPALIVE, CONNECTION_CLOSE };

typedef struct {
	int size;
	int current;
	char *mem;
} MemPool;

typedef struct {
	int size;
	int current;
	char **strings;
	MemPool *mp;
} StringPool;

// main
int main(void);
void *serverThread(void *);
void sigtermHandler(const int);
void sigchldHandler(const int);

// protocol
enum ConnectionState httpRequest(const int);

// io
void setTimeout(const int);
int parseHeader(const int, MemPool *, StringPool *);
ssize_t sendMemPool(const int, const MemPool *);
ssize_t sendBuffer(const int, const char *, const ssize_t);
ssize_t sendFile(const int, const int, const ssize_t);
ssize_t pipeToSocket(const int, const int, const ssize_t);
ssize_t pipeToFile(const int, const int, const ssize_t);

// mem
void memPoolReset(MemPool *);
void memPoolResetTo(MemPool *, int);
enum ErrorState memPoolAdd(MemPool *, const char *);
enum ErrorState memPoolExtend(MemPool *, const char *);
enum ErrorState memPoolExtendChar(MemPool *, const char);
enum ErrorState memPoolExtendNumber(MemPool *, const unsigned);
void memPoolReplace(MemPool *, const char, const char);
int memPoolLineBreak(const MemPool *, const int);
//
void stringPoolReset(StringPool *);
enum ErrorState stringPoolAdd(StringPool *, const char *);
enum ErrorState stringPoolAddVariable(StringPool *, const char *, const char *);
enum ErrorState stringPoolAddVariableNumber(StringPool *, const char *, const unsigned);
enum ErrorState stringPoolAddVariables(StringPool *, const StringPool *, const char*);
char *stringPoolReadVariable(const StringPool *, const char *);

// util
extern char digit[];
extern FILE *logFile;
int LogOpen(const int);
void LogClose(const int);
void Log(const int, const char *, ...);
const char *mimeType(const char *);
int hexDigit(const char);
enum ErrorState urlDecode(char *);
enum ErrorState fileNameEncode(const char *, char *, size_t);
enum ErrorState fileWriteChar(FILE *, const char);
enum ErrorState fileWriteNumber(FILE *, const unsigned);
enum ErrorState fileWriteString(FILE *, const char *);
enum ErrorState fileWriteDirectory(FILE *, MemPool *);
int openFileForWriting(MemPool *, char *);
char *strToLower(char *);
char *strToUpper(char *);
char *startOf(char *);

#endif
