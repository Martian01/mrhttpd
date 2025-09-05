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

#define RECEIVE_TIMEOUT 30
#define SEND_TIMEOUT 5

void setTimeout(const int socket) {
	struct timeval timeout;

	timeout.tv_sec  = RECEIVE_TIMEOUT;
	timeout.tv_usec = 0;
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	timeout.tv_sec  = SEND_TIMEOUT;
	timeout.tv_usec = 0;
	setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

int parseHeader(const int socket, MemPool* buffer, StringPool* headerPool) {
	ssize_t received;
	int cursor;
	int delim;
	int rejectCurrent = 0;

	stringPoolReset(headerPool);

_startHeader:
	memPoolReset(buffer);

_moreHeader:
	#if DEBUG & 8
	Log(socket, "parseHeader: read loop iteration.");
	#endif

	received = recv(socket, buffer->mem + buffer->current, buffer->size - buffer->current, 0);
	if (received == 0) {
		#if DEBUG & 8
		Log(socket, "parseHeader: recv strangeness. received=0");
		#endif
		return -1; // socket timeout
	}
	if (received < 0) {
		#if DEBUG & 8
		Log(socket, "parseHeader: recv error. received=%d, errno=%d", received, errno);
		#endif
		return received; // propagate error
	}
	#if DEBUG & 8
	Log(socket, "parseHeader: recv OK. received=%d", received);
	#endif
	buffer->current += received;
	cursor = 0; // cursor positioned on beginning of line

_nextHeaderLine:
	if (cursor >= buffer->current)
		goto _startHeader; // buffer exhausted, read more data
	delim = memPoolLineBreak(buffer, cursor); // search for end of line
	#if DEBUG & 16
	Log(socket, "parseHeader: parser loop iteration. cursor=%d, delim=%d, end=%d", cursor, delim, buffer->current);
	#endif
	if (delim < 0) { // end of line not found
		if (rejectCurrent) {
			#if DEBUG & 32
			Log(socket, "parseHeader: reject buffer with length: %d", buffer->current);
			#endif
			goto _startHeader;
		}
		if (cursor > 0) { // make some space and keep reading
			#if DEBUG & 32
			Log(socket, "parseHeader: move buffer by: %d", cursor);
			#endif
			buffer->current -= cursor;
			memmove(buffer->mem, buffer->mem + cursor, buffer->current);
			goto _moreHeader;
		}
		if (buffer->current < buffer->size) { // buffer not full: keep reading
			#if DEBUG & 32
			Log(socket, "parseHeader: top up buffer");
			#endif
			goto _moreHeader;
		}
		// buffer overflow
		if (headerPool->current > 0) {
			#if DEBUG & 32
			Log(socket, "parseHeader: beginning to reject at index: %d", cursor);
			#endif
			rejectCurrent = 1; // keep reading but skip the current header
			goto _startHeader;
		}
		return 0; // request line could not be read	
	}
	buffer->mem[delim] = '\0'; // make cursor line a zero-terminated string
	if (rejectCurrent) {
		#if DEBUG & 32
		Log(socket, "parseHeader: rejct chunk at index: %d", cursor);
		#endif
		rejectCurrent = 0;
		cursor = delim + 2; // beginning of next line
		goto _nextHeaderLine; // find next line in buffer
	}
	if (cursor != delim) { // line is not empty
		if (stringPoolAdd(headerPool, buffer->mem + cursor)) {
			if (headerPool->current == 0) { // request line
				#if DEBUG & 32
				Log(socket, "parseHeader: abort at index: %d", cursor);
				#endif
				return -1;
			}
			#if DEBUG & 32
			Log(socket, "parseHeader: skip header at index: %d", cursor);
			#endif
		}
		#if DEBUG & 32
		else { Log(socket, "parseHeader: recorded header at index: %d", cursor); }
		#endif
		cursor = delim + 2; // beginning of next line
		goto _nextHeaderLine; // find next line in buffer
	}

	// else: empty line found == end of header

	cursor = delim + 2;
	if (cursor < buffer->current) { // there is some data left in the buffer, most likely the beginning of the request body.
		buffer->current -= cursor;
		memmove(buffer->mem, buffer->mem + cursor, buffer->current); // return the overspill via the memory pool.
		#if DEBUG & 64
		Log(socket, "parseHeader: overspill returned, amount=%d", buffer->current);
		#endif
	} else { // no data left in the buffer
		memPoolReset(buffer); // initialize the buffer
		#if DEBUG & 64
		Log(socket, "parseHeader: spot landing (no overspill).");
		#endif
	}

	return headerPool->current; // 0 indicates an error: request line not found
}

ssize_t sendMemPool(const int socket, const MemPool* mp) {
	return sendBuffer(socket, mp->mem, mp->current);
}

ssize_t sendBuffer(const int socket, const char* buf, const ssize_t count) {
	ssize_t totalSent = 0, sent;

	while (totalSent < count) {
		#if DEBUG & 2
		Log(socket, "sendBuffer: loop iteration.");
		#endif
		sent = send(socket, buf + totalSent, count - totalSent, MSG_NOSIGNAL);
		if (sent == 0) {
			#if DEBUG & 2
			Log(socket, "sendBuffer: send strangeness. sent=%d", sent);
			#endif
			return -1; // timeout
		}
		if (sent < 0) {
			#if DEBUG & 2
			Log(socket, "sendBuffer: send error. sent=%d, errno=%d", sent, errno);
			#endif
			return sent; // propagate error
		}
		totalSent += sent;
		#if DEBUG & 2
		Log(socket, "sendBuffer: inside loop. sent=%d", sent);
		#endif
	}

	#if DEBUG & 2
	Log(socket, "sendBuffer: return OK. totalSent=%d", totalSent);
	#endif
	return totalSent;
}

#if USE_SENDFILE == 1

ssize_t sendFile(const int socket, const int fd, const ssize_t count) {
	ssize_t totalSent = 0, sent;

	while (totalSent < count) {
		#if DEBUG & 2
		Log(socket, "sendFile: loop iteration.");
		#endif

		sent = sendfile(socket, fd, null, count - totalSent);
		if (sent == 0) {
			#if DEBUG & 2
			Log(socket, "sendFile: pipe strangeness. sent=%d", sent);
			#endif
			return -1; // timeout
		}
		if (sent < 0 ) {
			#if DEBUG & 2
			Log(socket, "sendFile: pipe error. sent=%d, errno=%d", sent, errno);
			#endif
			return sent; // propagate error
		}
		#if DEBUG & 2
		Log(socket, "sendFile: inside loop. sent=%d", sent);
		#endif
		totalSent += sent;
	}

	#if DEBUG & 2
	Log(socket, "sendFile: return OK. totalSent=%d", totalSent);
	#endif
	return totalSent;
}

#else

ssize_t sendFile(const int socket, const int fd, const ssize_t count) {
	char buf[16384];
	ssize_t received, sent;
	ssize_t totalReceived = 0, totalSent = 0;

	#if DEBUG & 2
	Log(socket, "pipeToSocket: entering.");
	#endif
	while (totalReceived < count) {
		int toBeRead = count - totalReceived;
		received = read(fd, buf, toBeRead >= sizeof(buf) ? sizeof(buf) : toBeRead);
		if (received == 0) {
			#if DEBUG & 2
			Log(socket, "pipeToSocket: side exit. totalReceived=%d, totalSent=%d", totalReceived, totalSent);
			#endif
			return totalSent;
		}
		if (received < 0) {
			#if DEBUG & 2
			Log(socket, "pipeToSocket: read error. received=%d, errno=%d", received, errno);
			#endif
			return received; // propagate error
		}
		sent = sendBuffer(socket, buf, received);
		if (sent < 0) {
			#if DEBUG & 2
			Log(socket, "pipeToSocket: send error. sent=%d, errno=%d", sent, errno);
			#endif
			return sent; // propagate error
		}
		totalSent += sent;
		totalReceived += received;
	}
	#if DEBUG & 2
	Log(socket, "pipeToSocket: main exit. totalReceived=%d, totalSent=%d", totalReceived, totalSent);
	#endif
	return totalSent;
}

#endif

#ifdef PUT_PATH

ssize_t pipeToFile(const int socket, const int fd, const ssize_t count) {
	char buf[16384];
	ssize_t received, sent;
	ssize_t totalReceived = 0, totalSent = 0;

	#if DEBUG & 2
	Log(socket, "pipeToFile:s entering.");
	#endif
	while (totalReceived < count) {
		int toBeRead = count - totalReceived;
		received = recv(socket, buf, toBeRead >= sizeof(buf) ? sizeof(buf) : toBeRead, 0);
		if (received == 0) {
			#if DEBUG & 2
			Log(socket, "pipeToFile: side exit. totalReceived=%d, totalSent=%d", totalReceived, totalSent);
			#endif
			return totalSent;
		}
		if (received < 0) {
			#if DEBUG & 2
			Log(socket, "pipeToFile: recv error. received=%d, errno=%d", received, errno);
			#endif
			return received; // propagate error
		}
		sent = write(fd, buf, received);
		if (sent < 0) {
			#if DEBUG & 2
			Log(socket, "pipeToFile: write error. sent=%d, errno=%d", sent, errno);
			#endif
			return sent; // propagate error
		}
		totalSent += sent;
		totalReceived += received;
	}
	#if DEBUG & 2
			Log(socket, "pipeToFile: main exit. totalReceived=%d, totalSent=%d", totalReceived, totalSent);
	#endif
	return totalSent;
}

#endif
