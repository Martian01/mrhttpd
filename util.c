/*

mrhttpd v2.4.2
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

#ifdef EXT_FILE_CMD
#include <unistd.h>
#endif


// Log support

#if (LOG_LEVEL > 0) || (DEBUG > 0)

FILE *logfile;

pthread_mutex_t logfile_mutex = PTHREAD_MUTEX_INITIALIZER;

int Log_open(const int socket) {
	logfile = fopen(LOG_FILE, "at");
	if (logfile != NULL)
		Log(socket,
			"Server started. Port: " SERVER_PORT_STR "."
			#ifdef SYSTEM_USER
			" User: " SYSTEM_USER "."
			#endif
			#ifdef SERVER_ROOT
			" Server root: " SERVER_ROOT "."
			#endif
			);
	return logfile != NULL;
}

void Log_close(const int socket) {
	Log(socket,
		"Server exiting. Port: " SERVER_PORT_STR "."
		#ifdef SYSTEM_USER
		" User: " SYSTEM_USER "."
		#endif
		#ifdef SERVER_ROOT
		" Server root: " SERVER_ROOT "."
		#endif
		);
	fclose(logfile);
}

void Log(const int socket, const char *format, ...) {
	va_list ap;
	struct timeval tv;
	struct timezone tz;
	struct tm tm;

	pthread_mutex_lock( &logfile_mutex );

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm);
	fprintf(logfile, "%04d-%02d-%02d %02d:%02d:%02d.%06d  <%08d>  ", 
		tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, 
		tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec, socket);
	
	va_start(ap, format);
	vfprintf(logfile, format, ap);
	va_end(ap);

	fputs("\n", logfile);

	fflush(logfile);
	
	pthread_mutex_unlock( &logfile_mutex );

}
#endif


// MIME type support

// mime type detection based on suffix is a light-weight alternative to file(1)

const char *mimetype(const char *filename) {

	static const char *(assocNames[][2]) = {

/* the standard mime types should always be present */

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
		{ "ra",    "audio/x-pn-realaudio" },
		{ "ram",   "audio/x-pn-realaudio" },
		{ "mpeg",  "video/mpeg" },
		{ "mpg",   "video/mpeg" },
		{ "mpe",   "video/mpeg" },
		{ "avi",   "video/x-msvideo" },
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

	static const char *mime_default = "application/octet-stream";

	const char *((*anp)[2]) = NULL;
	char *suffix;

	suffix = strrchr(filename, '.');
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
	Log(0, "MT: calling external command for %s", filename);
	#endif
	int pipefd[2];
	static char buf[64];
	ssize_t cnt = 0;
	
	if (pipe(pipefd) == 0) {
		#if DEBUG & 128
		Log(0, "MT: pipe created witch fd %d %d", pipefd[0], pipefd[1]);
		#endif
		if (!fork()) { 
			// child process continues here
			if (dup2(pipefd[1], 1) == 1) {
				#if DEBUG & 128
				Log(0, "MT: child process execve \"%s -b --mime-type %s\"", EXT_FILE_CMD, filename);
				#endif
				execl(EXT_FILE_CMD, EXT_FILE_CMD, "-b", "--mime-type", filename, (char *) NULL); // should never return
				#if DEBUG & 128
				Log(0, "MT: child process returned with errno %d", errno);
				#endif
			}
			exit(-1);   // some error, exit child process
		}
		// parent process continues here
		close(pipefd[1]);
		cnt = read(pipefd[0], buf, 63);
		close(pipefd[0]);
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
	Log(0, "MT: returning default mime type %s", mime_default);
	#endif
	
	return mime_default;
}


// Support for hex encoding and decoding

int hexdigit(const char c) {
	static const char digits[] = "0123456789abcdef";
	const char *cp;

	for (cp = digits; *cp != '\0'; cp++)
		if (*cp == tolower(c))
			return cp - digits;
	return -1;
}

enum error_state url_decode(const char *in, char *out, size_t n) {
	if (in == NULL)
		return ERROR_FALSE; // allowed
	for (; (*out = *in) != '\0'; in++, out ++, n--) {
		if (n < 2)
			return ERROR_TRUE; // out of space
		if (*in == '%') {
			int lo, hi;
			hi = hexdigit(in[1]);
			if (hi >= 0) {
				lo = hexdigit(in[2]);
				if (lo >= 0) {
					*out = (hi<<4) + lo;
					in += 2;
				}
			}
		}
	}
	return ERROR_FALSE; // success
}

enum error_state filename_encode(const char *in, char *out, size_t n) {
	if (in == NULL)
		return ERROR_FALSE; // allowed
	for (; (*out = *in) != '\0'; in++, out ++, n--) {
		if (n < 2)
			return ERROR_TRUE; // out of space
		if (*in == '/') {
			if (n < 4)
				return ERROR_TRUE; // out of space
			*out++ = '%';
			*out++ = '2';
			*out   = 'F';
			n-=2;
		}
	}
	return ERROR_FALSE; // success
}


// Miscellaneous

char *str_tolower(char *string) {
	char *cp;
	
	if (string != NULL)
		for (cp = string; *cp; cp++)
			*cp = tolower(*cp);
	return string;
}

char *str_toupper(char *string) {
	char *cp;
	
	if (string != NULL)
		for (cp = string; *cp; cp++)
			*cp = toupper(*cp);
	return string;
}

char *startof(char *string) {
	if (string != NULL)
		while (*string != '\0' && !isgraph(*string))
			++string;
	return string;
}

