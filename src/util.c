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

const char digit[] = "0123456789abcdef";

char* strToLower(char* string) {
	char* cp;

	if (string != null)
		for (cp = string; *cp; cp++)
			*cp = tolower(*cp);
	return string;
}

char* strToUpper(char* string) {
	char* cp;

	if (string != null)
		for (cp = string; *cp; cp++)
			*cp = toupper(*cp);
	return string;
}

char* startOf(char* string) {
	if (string != null)
		while (*string != '\0' && !isgraph(*string))
			++string;
	return string;
}

// Formatted file output

#if (LOG_LEVEL > 0) || (DEBUG > 0) || (AUTO_INDEX > 0)

boolean fileWriteChar(FILE* file, const char c) {
	return fputc(c, file) != c;
}

boolean fileWriteNumber(FILE* file, const unsigned num) {
	return num < 10 ? fileWriteChar(file, digit[num]) : (fileWriteNumber(file, num / 10) || fileWriteChar(file, digit[num % 10]));
}

boolean fileWriteNumberFixed(FILE* file, const unsigned num, const unsigned lenght) {
	return lenght < 2 ? fileWriteChar(file, num < 10 ? digit[num] : '*') : fileWriteNumberFixed(file, num / 10, lenght - 1) || fileWriteChar(file, digit[num % 10]);
}

boolean fileWriteString(FILE* file, const char* str) {
	int len = strlen(str);
	return fwrite(str, 1, len, file) != len;
}

boolean fileWriteTimestamp(FILE* file, time_t ts) {
	struct tm tm;
	localtime_r(&ts, &tm);
	return
		fileWriteNumberFixed(file, tm.tm_year+1900, 4) ||
		fileWriteChar(file, '-') ||
		fileWriteNumberFixed(file, tm.tm_mon+1, 2) ||
		fileWriteChar(file, '-') ||
		fileWriteNumberFixed(file, tm.tm_mday, 2) ||
		fileWriteChar(file, ' ') ||
		fileWriteNumberFixed(file, tm.tm_hour, 2) ||
		fileWriteChar(file, ':') ||
		fileWriteNumberFixed(file, tm.tm_min, 2) ||
		fileWriteChar(file, ':') ||
		fileWriteNumberFixed(file, tm.tm_sec, 2)
	;
}

boolean fileWriteTimestampNow(FILE* file) {
	struct timeval tv;
	struct tm tm;
	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm);
	return
		fileWriteNumberFixed(file, tm.tm_year+1900, 4) ||
		fileWriteChar(file, '-') ||
		fileWriteNumberFixed(file, tm.tm_mon+1, 2) ||
		fileWriteChar(file, '-') ||
		fileWriteNumberFixed(file, tm.tm_mday, 2) ||
		fileWriteChar(file, ' ') ||
		fileWriteNumberFixed(file, tm.tm_hour, 2) ||
		fileWriteChar(file, ':') ||
		fileWriteNumberFixed(file, tm.tm_min, 2) ||
		fileWriteChar(file, ':') ||
		fileWriteNumberFixed(file, tm.tm_sec, 2) ||
		fileWriteChar(file, '.') ||
		fileWriteNumberFixed(file, (unsigned) tv.tv_usec, 6)
	;
}

#endif

// Support for hex encoding and decoding

int hexDigit(const char c) {
	const char* cp;

	const char lc = tolower(c);
	for (cp = digit; *cp != '\0'; cp++)
		if (*cp == lc)
			return cp - digit;
	return -1;
}

boolean urlDecode(char* buffer) {
	if (buffer == null)
		return false; // allowed
	char* in, *out;
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
	return false; // success
}

boolean fileNameEncode(const char* in, char* out, size_t outLength) {
	if (in == null)
		return false; // allowed
	for (; (*out = *in) != '\0'; in++, out ++, outLength--) {
		if (outLength < 2)
			return true; // out of space
		if (*in == '/') {
			if (outLength < 4)
				return true; // out of space
			*out++ = '%';
			*out++ = '2';
			*out   = 'F';
			outLength -= 2;
		}
	}
	return false; // success
}

// MIME type support

// mime type detection based on suffix is a light-weight alternative to file(1)

const char* mimeType(const char* fileName) {

	static const char* (assocNames[][2]) = {

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

		{ "yml",   "text/yaml" },
		{ "yaml",  "text/yaml" },
		{ "md",    "text/markdown" },
		{ "bmp",   "image/bmp" },
		{ "mp3",   "audio/mpeg" },
		{ "mp2",   "audio/mpeg" },
		{ "mpga",  "audio/mpeg" },
		{ "wav",   "audio/x-wav" },
		{ "mid",   "audio/midi" },
		{ "midi",  "audio/midi" },
		{ "mpeg",  "video/mpeg" },
		{ "mpg",   "video/mpeg" },
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
		
/* the null string must be the last entry */

		{ null,   null }
	};

	static const char* mimeDefault = "application/octet-stream";

	const char* ((*anp)[2]) = null;
	char* suffix;

	suffix = strrchr(fileName, '.');
	if (suffix != null) {
		suffix++;
		for (anp = assocNames; (*anp)[0] != null; anp++)
			if (!strcmp((*anp)[0], suffix)) {
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
				execl(EXT_FILE_CMD, EXT_FILE_CMD, "-b", "--mime-type", fileName, null); // should never return
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
		while ((cnt > 0) && iscntrl(buf[cnt - 1]))
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

// Log support

#if (LOG_LEVEL > 0) || (DEBUG > 0)

FILE* logFile;

pthread_mutex_t logFileMutex = PTHREAD_MUTEX_INITIALIZER;

int LogOpen(const int socket) {
	#ifdef LOG_FILE
	logFile = fopen(LOG_FILE, "at");
	#else
	logFile = stdout;
	#endif
	if (logFile != null)
		Log(socket,
			"Server started. Port: " SERVER_PORT_STR "."
			#ifdef SYSTEM_USER
			" User: " SYSTEM_USER "."
			#endif
			#ifdef SERVER_ROOT
			" Server root: " SERVER_ROOT "."
			#endif
			);
	return logFile != null;
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

void Log(const int socket, const char* format, ...) {
	va_list ap;
	pthread_mutex_lock(&logFileMutex);
	fileWriteTimestampNow(logFile);
	fileWriteString(logFile, "  <");
	fileWriteNumberFixed(logFile, socket, 8);
	fileWriteString(logFile, ">  ");
	va_start(ap, format);
	vfprintf(logFile, format, ap);
	va_end(ap);
	fileWriteChar(logFile, '\n');
	fflush(logFile);
	pthread_mutex_unlock(&logFileMutex);
}

#endif

// PUT & DELETE support

#ifdef PUT_PATH

int isTrueDirectory(char* fileName) {
	return
		fileName != null
			&&
		(fileName[0] != '\0')
			&&
		(fileName[0] != '.' || fileName[1] != '\0')
			&&
		(fileName[0] != '.' || fileName[1] != '.' || fileName[2] != '\0')
			;
}

int openFileForWriting(MemPool* fileNamePool, char* resource) {
	char* token;
	struct stat st;
	int file;

	while(1) {
		token = strsep(&resource, "/");
		#if DEBUG & 1024
		Log(0, "OFFW: token=\"%s\" resource=\"%s\"", token, resource);
		#endif
		if (token == null)
			return true; // bad format
		if (*token == '\0')
			continue; // eat leading slash
		if (memPoolExtendChar(fileNamePool, '/') || memPoolExtend(fileNamePool, token))
			return true; // out of memory
		if (resource == null || *resource == '\0') {
			#if DEBUG & 1024
			Log(0, "OFFW: file=\"%s\"", fileNamePool->mem);
			#endif
			return open(fileNamePool->mem, O_CREAT | O_TRUNC | O_WRONLY, 0755); // open file for writing
		}
		if (stat(fileNamePool->mem, &st)) {
			if (mkdir(fileNamePool->mem, 0755)) // path component does not exist, create directory
				return true; // mkdir failed
		} else if (!S_ISDIR(st.st_mode))
			return true; // path component exists but is not a directory
	}
}

boolean deleteFileTree(MemPool* fileNamePool) {
	struct stat st;
	struct dirent* dp;
	if (stat(fileNamePool->mem, &st))
		return false; // file does not exist
	if (S_ISDIR(st.st_mode)) { // directory must be empty
		// normalize directory name
		if (fileNamePool->current > 1 && fileNamePool->mem[fileNamePool->current - 2] != '/')
			if (memPoolExtendChar(fileNamePool, '/')) 
				return true;
		DIR* dir = opendir(fileNamePool->mem);
		if (dir == null)
			return true;
		int savePosition = fileNamePool->current;
		while ((dp = readdir(dir)) != null) {
			if (isTrueDirectory(dp->d_name)) {
				if (memPoolExtend(fileNamePool, dp->d_name) || deleteFileTree(fileNamePool)) {
					//memPoolResetTo(fileNamePool, savePosition);
					closedir(dir);
					return true;
				}
				memPoolResetTo(fileNamePool, savePosition);
			}
		}
		closedir(dir);
	}
	return remove(fileNamePool->mem) < 0 ? true : false;
}

#endif

#if AUTO_INDEX > 0

int isNavigationTarget(char* fileName) {
	return
		fileName != null
			&&
		(fileName[0] != '\0')
			&&
		(fileName[0] != '.' || fileName[1] != '\0')
			;
}

boolean fileWriteDirectory(FILE* file, MemPool* fileNamePool, char* resource) {
	struct dirent* dp;
	int found = 0;

	#if DEBUG & 1024
	Log(0, "FWD: mem=\"%s\", current=%d", fileNamePool->mem, fileNamePool->current);
	#endif
	// assumption: file name ends with '/'
	int savePosition = fileNamePool->current;
	DIR* dir = opendir(fileNamePool->mem);
	if (dir == null)
		return true;
	if (
		#if AUTO_INDEX == 1
		fileWriteString(file, "{\"path\":\"") ||
		fileWriteString(file, resource) ||
		fileWriteString(file, "\",\"timestamp\":\"") ||
		fileWriteTimestampNow(file) ||
		fileWriteString(file, "\",\"server\":\"") ||
		fileWriteString(file, SERVER_SOFTWARE) ||
		fileWriteString(file, "\",\"entries\":[")
		#else
		fileWriteString(file, "<?xml version=\"1.0\"?>") ||
		#ifdef XSLT_HEADER
		fileWriteString(file, XSLT_HEADER) ||
		#endif
		fileWriteString(file, "<directory><path>") ||
		fileWriteString(file, resource) ||
		fileWriteString(file, "</path><timestamp>") ||
		fileWriteTimestampNow(file) ||
		fileWriteString(file, "</timestamp><server>") ||
		fileWriteString(file, SERVER_SOFTWARE) ||
		fileWriteString(file, "</server>")
		#endif
	) {
		closedir(dir);
		return true;
	}
	while ((dp = readdir(dir)) != null) {
		if (isNavigationTarget(dp->d_name)) {
			struct stat st;
			if (memPoolExtend(fileNamePool, dp->d_name)) {
				closedir(dir);
				return true;
			}
			if (stat(fileNamePool->mem, &st) || (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode))) {
				#if DEBUG & 1
				Log(0, "FWD: skipping %s", fileNamePool->mem);
				#endif
				memPoolResetTo(fileNamePool, savePosition);
				continue;
			}
			memPoolResetTo(fileNamePool, savePosition);
			if (
				#if AUTO_INDEX == 1
				(found++ > 0 && fileWriteChar(file, ',')) ||
				fileWriteString(file, "{\"name\":\"") ||
				fileWriteString(file, dp->d_name) ||
				fileWriteString(file, "\",\"type\":\"") ||
				fileWriteString(file, S_ISDIR(st.st_mode) ? "dir" : mimeType(dp->d_name)) ||
				fileWriteString(file, "\",\"size\":\"") ||
				fileWriteNumber(file, (unsigned) st.st_size) ||
				fileWriteString(file, "\",\"modified\":\"") ||
				fileWriteTimestamp(file, st.st_mtime) ||
				fileWriteString(file, "\"}")
				#else
				fileWriteString(file, "<entry><name>") ||
				fileWriteString(file, dp->d_name) ||
				fileWriteString(file, "</name><type>") ||
				fileWriteString(file, S_ISDIR(st.st_mode) ? "dir" : mimeType(dp->d_name)) ||
				fileWriteString(file, "</type><size>") ||
				fileWriteNumber(file, (unsigned) st.st_size) ||
				fileWriteString(file, "</size><modified>") ||
				fileWriteTimestamp(file, st.st_mtime) ||
				fileWriteString(file, "</modified></entry>")
				#endif
			) {
				closedir(dir);
				return true;
			}

		}
	}
	closedir(dir);
	return
		#if AUTO_INDEX == 1
		fileWriteString(file, "]}")
		#else
		fileWriteString(file, "</directory>")
		#endif
	;
}

#endif
