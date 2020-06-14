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

#if SENDFILE == 1
#include <sys/sendfile.h>
#endif

#define RECEIVE_TIMEOUT 30
#define SEND_TIMEOUT 5

void set_timeout(const int sockfd) {
	struct timeval timeout;

	timeout.tv_sec  = RECEIVE_TIMEOUT;
	timeout.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	timeout.tv_sec  = SEND_TIMEOUT;
	timeout.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

#define RECV_BUFLEN 2047

int recv_header_with_timeout(stringpool *sp, const int sockfd) {
	char buf[RECV_BUFLEN + 1];
	ssize_t numbytes; 
	ssize_t received;
	fd_set readfds;
	struct timeval timeout;
	char *cursor;
	char *delim;

	sp_reset(sp);
	numbytes = 0;

_read:
	#if DEBUG & 8
	Log(sockfd, "RHWT: read loop iteration.");
	#endif

	received = recv(sockfd, buf + numbytes, RECV_BUFLEN - numbytes, 0);
	if (received == 0) {
		#if DEBUG & 8
		Log(sockfd, "RHWT: recv strangeness. received=%d", received);
		#endif
		return -ESTRANGE; // error
	}
	if (received < 0) {
		#if DEBUG & 8
		Log(sockfd, "RHWT: recv error. received=%d, errno=%d", received, errno);
		#endif
		if (errno == EAGAIN)
			goto _read;
		return received; // propagate error
	}
	#if DEBUG & 8
	Log(sockfd, "RHWT: recv OK. received=%d", received);
	#endif
	numbytes += received;
	buf[numbytes] = '\0'; // end marker for strstr
		
	cursor = buf; // cursor positioned on beginning of line
_parse:
	delim = strstr(cursor, "\r\n"); // search for end of line
	#if DEBUG & 16
	Log(sockfd, "RHWT: parser loop iteration. numbytes=%#x, buf=%p, cursor=%p, delim=%p", 
		numbytes, buf, cursor, delim);
	#endif
	if (delim == NULL) { // end of line not found
		if (cursor > buf) { // make some space and keep reading
			numbytes -= (cursor - buf);
			memmove(buf, cursor, numbytes);
			goto _read;
		}
		if (numbytes < RECV_BUFLEN) { // buffer not full: keep reading
			goto _read;
		}
		return -1; // buffer full: we're bloated
	}
	else if (cursor != delim) { // line is not empty
		*delim = '\0'; // make it a zero terminated string
		if (sp_add(sp, cursor)) // add string to header array
		  return -1; // header array full or whatever
		#if DEBUG & 32
		Log(sockfd, "RHWT: parser found header: %s", cursor);
		#endif
		cursor = delim + 2; // beginning of next line
		if (cursor < buf + numbytes) {
			goto _parse; // find next line in buffer
		}
		else {
			numbytes = 0; // buffer exhausted, read more data
			goto _read;
		}
	}
		
	// empty line found == end of header

	// The question at this point is whether there is any further data
	// after the header, and if so, to which HTTP request it belongs:
	// the current one or the next one?
	// At this point we assume it belongs to the current one (probably
	// some misguided POST or PUT request) and can be discarded.
	// Since we currently only support the GET method we do not attempt
	// to read any further data after the header. 

	#if DEBUG & 64
	cursor = delim + 2;
	if (cursor < buf + numbytes) { // there is some data left in the buffer
		numbytes -= (cursor - buf); // save the data in the buffer
		memmove(buf, cursor, numbytes);
		buf[32] = '\0'; // hack for logging purposes only
		Log(sockfd, "RHWT: data past header: numbytes=%#x, data=%s", numbytes, buf );
		return 0;
	} else { // spot landing: no data left in buffer
		numbytes = 0; // initialize the buffer
		Log(sockfd, "RHWT: spot landing (no data past header).");
		return 0;
	}
	#endif

	return 0; // assume success
}

ssize_t send_with_timeout(const int sockfd, const char *buf, const ssize_t len) {
	ssize_t numbytes, sent, remaining;
	fd_set writefds;
	struct timeval timeout;

	numbytes = 0;
	remaining = len;

	do {
		#if DEBUG & 2
		Log(sockfd, "SWT:  loop iteration.");
		#endif
		sent = send(sockfd, buf + numbytes, remaining, MSG_NOSIGNAL);
		if (sent == 0) {
			#if DEBUG & 2
			Log(sockfd, "SWT:  send strangeness. sent=%d", sent);
			#endif
			return -ESTRANGE; // error
		}
		if (sent < 0) {
			#if DEBUG & 2
			Log(sockfd, "SWT:  send error. sent=%d, errno=%d", sent, errno);
			#endif
			if (errno == EAGAIN)
				continue;
			return sent; // propagate error
		}
		numbytes += sent;
		remaining -= sent;
		#if DEBUG & 2
		Log(sockfd, "SWT:  send OK. sent=%d", sent);
		#endif
	}
	while (remaining > 0);

	#if DEBUG & 2
	Log(sockfd, "SWT:  return OK. numbytes=%d", numbytes);
	#endif
	return numbytes;
}

#if SENDFILE == 0

// simple replacement of sendfile() for non-linux builds
// note: offset functionality has been stripped from signature and code

#define SF_BUFLEN 16384

ssize_t mysendfile(const int sockfd, const int fd, ssize_t count) {
	char buf[SF_BUFLEN];
	ssize_t received, sent;
	ssize_t total = 0;
	
	#if DEBUG & 2
	Log(sockfd, "MSF:  entering.");
	#endif
	do {
		received = read(fd, buf, (count >= SF_BUFLEN) ? SF_BUFLEN : count );
		if (received == 0) {
			#if DEBUG & 2
			Log(sockfd, "MSF:  read strangeness. received=%d", received);
			#endif
			return -ESTRANGE; // error
		}
		if (received < 0) {
			#if DEBUG & 2
			Log(sockfd, "MSF:  read error. received=%d, errno=%d", received, errno);
			#endif
			return received; // propagate error
		}
		sent = send_with_timeout(sockfd, buf, received);
		if (sent < 0) {
			#if DEBUG & 2
			Log(sockfd, "MSF:  SWT error. sent=%d", sent);
			#endif
			return sent; // propagate error
		}
		count -= received;
		total += received;
	} while (count > 0);
	
	#if DEBUG & 2
	Log(sockfd, "MSF:  return OK. total=%d", total);
	#endif
	return total;
}

#endif

ssize_t sendfile_with_timeout(const int sockfd, const int fd, const ssize_t len) {
	ssize_t numbytes, remaining, sent;

	numbytes = 0;
	remaining = len;

	do {
		#if DEBUG & 2
		Log(sockfd, "SFWT: loop iteration.");
		#endif
		
		#if SENDFILE == 0
		sent = mysendfile(sockfd, fd, remaining);
		#endif
		#if SENDFILE == 1
		sent = sendfile(sockfd, fd, NULL, remaining);
		#endif
		if (sent == 0) {
			#if DEBUG & 2
			Log(sockfd, "SFWT: sendfile strangeness. sent=%d", sent);
			#endif
			return -ESTRANGE; // error
		}
		if (sent < 0 ) {
			#if DEBUG & 2
			Log(sockfd, "SFWT: sendfile error. sent=%d, errno=%d", sent, errno);
			#endif
			#if SENDFILE == 1
			if (errno == EAGAIN)
				continue;
			#endif
			return sent; // propagate error
		}
		numbytes += sent;
		remaining -= sent;
		#if DEBUG & 2
		Log(sockfd, "SFWT: sendfile OK. sent=%d", sent);
		#endif
	}
	while (remaining > 0);

	#if DEBUG & 2
	Log(sockfd, "SFWT: return OK. numbytes=%d", numbytes);
	#endif
	return numbytes;
}

