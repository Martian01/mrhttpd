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

#include "mrhttpd.h"

char digit[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

// Log support

#if (LOG_LEVEL > 0) || (DEBUG > 0)

FILE *logFile;

pthread_mutex_t logFileMutex = PTHREAD_MUTEX_INITIALIZER;

int LogOpen(const int socket) {
	#ifdef LOG_FILE
	logFile = fopen(LOG_FILE, "at");
	#else
	logFile = stdout;
	#endif
	if (logFile != NULL)
		Log(socket,
			"Server started. Port: " SERVER_PORT_STR "."
			#ifdef SYSTEM_USER
			" User: " SYSTEM_USER "."
			#endif
			#ifdef SERVER_ROOT
			" Server root: " SERVER_ROOT "."
			#endif
			);
	return logFile != NULL;
}

void LogClose(const int socket) {
	Log(socket,
		"Server exiting. Port: " SERVER_PORT_STR "."
		#ifdef SYSTEM_USER
		" User: " SYSTEM_USER "."
		#endif
		#ifdef SERVER_ROOT
		" Server root: " SERVER_ROOT "."
		#endif
		);
	fclose(logFile);
}

void Log(const int socket, const char *format, ...) {
	va_list ap;
	struct timeval tv;
	struct timezone tz;
	struct tm tm;

	pthread_mutex_lock( &logFileMutex );

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm);
	fprintf(logFile, "%04d-%02d-%02d %02d:%02d:%02d.%06d  <%08d>  ", 
		tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, 
		tm.tm_hour, tm.tm_min, tm.tm_sec, (int) tv.tv_usec, socket);
	
	va_start(ap, format);
	vfprintf(logFile, format, ap);
	va_end(ap);

	fputs("\n", logFile);

	fflush(logFile);
	
	pthread_mutex_unlock( &logFileMutex );

}
#endif


// MIME type support

// mime type detection based on suffix is a light-weight alternative to file(1)

const char *mimeType(const char *fileName) {

	static const char *(assocNames[][2]) = {

/* the mime types for web content should always be present */

		{ "html",  "text/html" },
		{ "htm",   "text/html" },
		{ "txt",   "text/plain" },
		{ "css",   "text/css" },
		{ "png",   "image/png" },
		{ "gif",   "image/gif" },
		{ "jpg",   "image/jpeg" },
		{ "jpeg",  "image/jpeg" },
		{ "ico",   "image/vnd.microsoft.icon" },
		{ "js",    "application/x-javascript" },
		
/* the following can be modified as required */

		{ "bmp",   "image/bmp" },
		{ "mp3",   "audio/mpeg" },
		{ "mp2",   "audio/mpeg" },
		{ "mpga",  "audio/mpeg" },
		{ "wav",   "audio/x-wav" },
		{ "mid",   "audio/midi" },
		{ "midi",  "audio/midi" },
		{ "mpeg",  "video/mpeg" },
		{ "mpg",   "video/mpeg" },
		{ "mpe",   "video/mpeg" },
		{ "mp4",   "video/mp4" },
		{ "3gp",   "video/3gpp" },
		{ "mov",   "video/quicktime" },
		{ "wmv",   "video/x-ms-wmv" },
		{ "avi",   "video/x-msvideo" },
		{ "json",  "application/json" },
		{ "xml",   "application/xml" },
		{ "xsl",   "application/xml" },
		{ "xslt",  "application/xslt-xml" },
		{ "xhtml", "application/xhtml+xml" },
		{ "xht",   "application/xhtml+xml" },
		{ "dtd",   "application/xml-dtd" },
		{ "tar",   "application/x-tar" },
		{ "zip",   "application/x-zip" },
		
/* the empty string MUST be the last entry */

		{ "",      "" }
	};

	static const char *mimeDefault = "application/octet-stream";

	const char *((*anp)[2]) = NULL;
	char *suffix;

	suffix = strrchr(fileName, '.');
	if (suffix != NULL) {
		suffix++;
		for (anp=assocNames; *((*anp)[0]); anp++)
			if (!strcmp((*anp)[0],suffix)) {
				return (*anp)[1];
			}
	}
	// suffix not found

	#ifdef EXT_FILE_CMD
	// call external command to determine file type
	// typically "file -b --mime-type"
	#if DEBUG & 128
	Log(0, "MT: calling external command for %s", fileName);
	#endif
	int pipeFd[2];
	static char buf[64];
	ssize_t cnt = 0;
	
	if (pipe(pipeFd) == 0) {
		#if DEBUG & 128
		Log(0, "MT: pipe created witch fd %d %d", pipeFd[0], pipeFd[1]);
		#endif
		if (!fork()) { 
			// child process continues here
			if (dup2(pipeFd[1], 1) == 1) {
				#if DEBUG & 128
				Log(0, "MT: child process execve \"%s -b --mime-type %s\"", EXT_FILE_CMD, fileName);
				#endif
				execl(EXT_FILE_CMD, EXT_FILE_CMD, "-b", "--mime-type", fileName, (char *) NULL); // should never return
				#if DEBUG & 128
				Log(0, "MT: child process returned with errno %d", errno);
				#endif
			}
			exit(-1);   // some error, exit child process
		}
		// parent process continues here
		close(pipeFd[1]);
		cnt = read(pipeFd[0], buf, 63);
		close(pipeFd[0]);
		// eat trailing whitespace
		while ( (cnt > 0) && iscntrl(buf[cnt-1]) )
			cnt--;
		buf[cnt] = '\0';
		#if DEBUG & 128
		Log(0, "MT: parent process obtained \"%s\"", buf);
		#endif
		if (*buf)
			return buf;
	}
	#endif
	#if DEBUG & 128
	Log(0, "MT: returning default mime type %s", mimeDefault);
	#endif
	
	return mimeDefault;
}


// Support for hex encoding and decoding

int hexDigit(const char c) {
	static const char digits[] = "0123456789abcdef";
	const char *cp;

	for (cp = digits; *cp != '\0'; cp++)
		if (*cp == tolower(c))
			return cp - digits;
	return -1;
}

enum ErrorState urlDecode(char *buffer) {
	if (buffer == NULL)
		return ERROR_FALSE; // allowed
	char *in, *out;
	in = out = buffer;
	for (; (*out = *in) != '\0'; in++, out++) {
		if (*in == '%') {
			int lo, hi;
			hi = hexDigit(in[1]);
			if (hi >= 0) {
				lo = hexDigit(in[2]);
				if (lo >= 0) {
					*out = (hi<<4) + lo;
					in += 2;
				}
			}
		}
	}
	return ERROR_FALSE; // success
}

enum ErrorState fileNameEncode(const char *in, char *out, size_t outLength) {
	if (in == NULL)
		return ERROR_FALSE; // allowed
	for (; (*out = *in) != '\0'; in++, out ++, outLength--) {
		if (outLength < 2)
			return ERROR_TRUE; // out of space
		if (*in == '/') {
			if (outLength < 4)
				return ERROR_TRUE; // out of space
			*out++ = '%';
			*out++ = '2';
			*out   = 'F';
			outLength -= 2;
		}
	}
	return ERROR_FALSE; // success
}

// Miscellaneous

#ifdef PUT_PATH
int openFileForWriting(MemPool *fileNamePool, char *resource) {
	char *token;
	struct stat st;
	int file;

	while(1) {
		token = strsep(&resource, "/");
		#if DEBUG & 1024
		Log(0, "OFFW: token=\"%s\" resource=\"%s\"", token, resource);
		#endif
		if (token == NULL)
			return ERROR_TRUE; // bad format
		if (*token == '\0')
			continue; // eat leading slash
		if (memPoolExtendChar(fileNamePool, '/') || memPoolExtend(fileNamePool, token))
			return ERROR_TRUE; // out of memory
		if (resource == NULL || *resource == '\0') {
			#if DEBUG & 1024
			Log(0, "OFFW: file=\"%s\"", fileNamePool->mem);
			#endif
			return open(fileNamePool->mem, O_CREAT | O_TRUNC | O_WRONLY, 0755); // open file for writing
		}
		if (stat(fileNamePool->mem, &st)) {
			if (mkdir(fileNamePool->mem, 0755)) // path component does not exist, create directory
				return ERROR_TRUE; // mkdir failed
		} else if (!S_ISDIR(st.st_mode))
			return ERROR_TRUE; // path component exists but is not a directory
	}
}
#endif

#if AUTO_INDEX == 1
enum ErrorState fileWriteChar(FILE *file, const char c) {
	return fputc(c, file) != c;
}

enum ErrorState fileWriteNumber(FILE *file, const unsigned num) {
	return num < 10 ? fileWriteChar(file, digit[num]) : (fileWriteNumber(file, num / 10) || fileWriteChar(file, digit[num % 10]));
}

enum ErrorState fileWriteString(FILE *file, const char *str) {
	int len = strlen(str);
	return fwrite(str, 1, len, file) != len;
}

enum ErrorState fileWriteDirectory(FILE *file, MemPool *fileNamePool) {
	struct dirent *dp;
	int found = 0;

	#if DEBUG & 1024
	Log(0, "FWD: mem=\"%s\", current=%d", fileNamePool->mem, fileNamePool->current);
	#endif
	//if (fileNamePool->current < 2)
	//	return ERROR_TRUE;
	//if (fileNamePool->mem[fileNamePool->current - 2] != '/' && memPoolExtendChar(fileNamePool, '/'))
	//	return ERROR_TRUE;
	int savePosition = fileNamePool->current;
	DIR *dir = opendir(fileNamePool->mem);
	if (dir == NULL)
		return ERROR_TRUE;

	if (fileWriteChar(file, '['))
		return ERROR_TRUE;
	while ((dp = readdir(dir)) != NULL) {
		if (strncmp(dp->d_name, ".", 1)) {
			memPoolExtend(fileNamePool, dp->d_name);
			struct stat st;
			if (stat(fileNamePool->mem, &st) < 0)
				return ERROR_TRUE;
			memPoolResetTo(fileNamePool, savePosition);
			unsigned fileSize = st.st_size;
			const char *fileType = S_ISDIR(st.st_mode) ? "dir" : (S_ISREG(st.st_mode) ? mimeType(dp->d_name) : "unknown");
			if (found++ > 0 && fileWriteChar(file, ','))
				return ERROR_TRUE;
			if (
				fileWriteChar(file, '{') ||
				fileWriteString(file, "\"name\":\"") ||
				fileWriteString(file, dp->d_name) ||
				fileWriteString(file, "\",") ||
				fileWriteString(file, "\"type\":\"") ||
				fileWriteString(file, fileType) ||
				fileWriteString(file, "\",") ||
				fileWriteString(file, "\"size\":") ||
				fileWriteNumber(file, fileSize) ||
				fileWriteChar(file, '}')
			)
				return ERROR_TRUE;
		}
	}
	if (fileWriteChar(file, ']'))
		return ERROR_TRUE;
	return ERROR_FALSE;
}
#endif

char *strToLower(char *string) {
	char *cp;
	
	if (string != NULL)
		for (cp = string; *cp; cp++)
			*cp = tolower(*cp);
	return string;
}

char *strToUpper(char *string) {
	char *cp;
	
	if (string != NULL)
		for (cp = string; *cp; cp++)
			*cp = toupper(*cp);
	return string;
}

char *startOf(char *string) {
	if (string != NULL)
		while (*string != '\0' && !isgraph(*string))
			++string;
	return string;
}
