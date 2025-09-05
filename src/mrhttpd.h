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


#ifndef MRHTTPD_INCLUDE
#define MRHTTPD_INCLUDE

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

#if AUTO_INDEX > 0
#include <dirent.h>
#endif

#if USE_SENDFILE == 1
#include <sys/sendfile.h>
#endif

#define SERVER_NAME       "mrhttpd"
#define SERVER_SOFTWARE   "mrhttpd/2.8.0"

#define PROTOCOL_HTTP_1_0 "HTTP/1.0"
#define PROTOCOL_HTTP_1_1 "HTTP/1.1"

typedef enum { false, true } boolean;

typedef enum { CONNECTION_KEEPALIVE, CONNECTION_CLOSE } ConnectionState;

typedef struct {
	int size;
	int current;
	char* mem;
} MemPool;

typedef struct {
	int size;
	int current;
	char** strings;
	MemPool* mp;
} StringPool;

#define null ((void*) 0L)

// main.c

extern char* authHeader;
extern int authMethods;

int main(void);
void*serverThread(void*);
void reaper();
void shutDownServer();
void sigTermHandler(const int);
void sigIntHandler(const int);
void sigHupHandler(const int);
void sigChldHandler(const int);

// protocol.c

ConnectionState httpRequest(const int);

// io.c

void setTimeout(const int);
int parseHeader(const int, MemPool*, StringPool*);
ssize_t sendMemPool(const int, const MemPool*);
ssize_t sendBuffer(const int, const char* , const ssize_t);
ssize_t sendFile(const int, const int, const ssize_t);
ssize_t pipeToSocket(const int, const int, const ssize_t);
ssize_t pipeToFile(const int, const int, const ssize_t);

// mem.c

void memPoolReset(MemPool*);
void memPoolResetTo(MemPool*, int);
int memPoolNextTarget(MemPool*);
boolean memPoolAdd(MemPool*, const char*);
boolean memPoolExtend(MemPool*, const char*);
boolean memPoolExtendChar(MemPool*, const char);
boolean memPoolExtendNumber(MemPool*, const unsigned);
void memPoolReplace(MemPool*, const char, const char);
int memPoolLineBreak(const MemPool*, const int);
void stringPoolReset(StringPool*);
boolean stringPoolAdd(StringPool*, const char*);
boolean stringPoolAddVariable(StringPool*, const char*, const char*);
boolean stringPoolAddVariableNumber(StringPool*, const char*, const unsigned);
boolean stringPoolAddVariables(StringPool*, const StringPool*, const char*);
char* stringPoolReadHttpHeader(const StringPool*, const char*);
char* removePrefix(const char*, char*);

// util.c

extern const char digit[];
extern FILE* logFile;

char* strToLower(char*);
char* strToUpper(char*);
char* startOf(char*);
boolean fileWriteChar(FILE*, const char);
boolean fileWriteNumber(FILE*, const unsigned);
boolean fileWriteString(FILE*, const char*);
boolean fileWriteTimestamp(FILE*, time_t);
boolean fileWriteTimestampNow(FILE*);
int hexDigit(const char);
boolean urlDecode(char*);
boolean fileNameEncode(const char*, char*, size_t);
const char* mimeType(const char*);
int LogOpen(const int);
void LogClose(const int);
void Log(const int, const char*, ...);
int openFileForWriting(MemPool*, char*);
boolean deleteFileTree(MemPool*);
boolean fileWriteDirectory(FILE* , MemPool*, char*);

#endif
