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

int receiveHeader(const int socket, StringPool *headerPool, MemPool *buffer) {
	ssize_t received;
	int cursor;
	int delim;

	stringPoolReset(headerPool);

_readFresh:
	memPoolReset(buffer);

_readMore:
	#if DEBUG & 8
	Log(socket, "receiveHeader: read loop iteration.");
	#endif

	received = recv(socket, buffer->mem + buffer->current, buffer->size - buffer->current, 0);
	if (received == 0) {
		#if DEBUG & 8
		Log(socket, "receiveHeader: recv strangeness. received=%d", received);
		#endif
		return -ESTRANGE; // error
	}
	if (received < 0) {
		#if DEBUG & 8
		Log(socket, "receiveHeader: recv error. received=%d, errno=%d", received, errno);
		#endif
		//if (errno == EAGAIN)
		//	goto _readMore;
		return received; // propagate error
	}
	#if DEBUG & 8
	Log(socket, "receiveHeader: recv OK. received=%d", received);
	#endif
	buffer->current += received;
	cursor = 0; // cursor positioned on beginning of line

_parseNextLine:
	delim = memPoolLineBreak(buffer, cursor); // search for end of line
	#if DEBUG & 16
	Log(socket, "receiveHeader: parser loop iteration. cursor=%#x, delim=%#x, end=%#x", cursor, delim, buffer->current);
	#endif
	if (delim < 0) { // end of line not found
		if (cursor > 0) { // make some space and keep reading
			buffer->current -= cursor;
			memmove(buffer->mem, buffer->mem + cursor, buffer->current);
			goto _readMore;
		}
		if (buffer->current < buffer->size) { // buffer not full: keep reading
			goto _readMore;
		}
		return -1; // buffer full: we're bloated
	} else if (cursor != delim) { // line is not empty
		buffer->mem[delim] = '\0'; // make it a zero-terminated string
		if (stringPoolAdd(headerPool, buffer->mem + cursor)) // add string to header pool
		  return -1; // header pool full or whatever
		#if DEBUG & 32
		Log(socket, "receiveHeader: parser found header: %#x", cursor);
		#endif
		cursor = delim + 2; // beginning of next line
		if (cursor < buffer->current)
			goto _parseNextLine; // find next line in buffer
		else
			goto _readFresh; // buffer exhausted, read more data
	}
		
	// else: empty line found == end of header

	cursor = delim + 2;
	if (cursor < buffer->current) { // there is some data left in the buffer, most likely the beginning of the request body.
		buffer->current -= cursor;
		memmove(buffer->mem, buffer->mem + cursor, buffer->current); // return the overspill via the memory pool.
		#if DEBUG & 64
		Log(socket, "receiveHeader: data past header: amount=%#x", buffer->current);
		#endif
	} else { // no data left in the buffer
		memPoolReset(buffer); // initialize the buffer
		#if DEBUG & 64
		Log(socket, "receiveHeader: spot landing (no data past header).");
		#endif
	}

	return headerPool->current > 0 ? 0 : -1; // success if and only if there is a non-empty header line
}

ssize_t sendMemPool(const int socket, const MemPool *mp) {
	return sendBuffer(socket, mp->mem, mp->current);
}

ssize_t sendBuffer(const int socket, const char *buf, const ssize_t len) {
	ssize_t numBytes, sent, remaining;

	numBytes = 0;
	remaining = len;

	while (remaining > 0) {
		#if DEBUG & 2
		Log(socket, "sendBuffer:  loop iteration.");
		#endif
		sent = send(socket, buf + numBytes, remaining, MSG_NOSIGNAL);
		if (sent == 0) {
			#if DEBUG & 2
			Log(socket, "sendBuffer:  send strangeness. sent=%d", sent);
			#endif
			return -ESTRANGE; // error
		}
		if (sent < 0) {
			#if DEBUG & 2
			Log(socket, "sendBuffer:  send error. sent=%d, errno=%d", sent, errno);
			#endif
			//if (errno == EAGAIN)
			//	continue;
			return sent; // propagate error
		}
		numBytes += sent;
		remaining -= sent;
		#if DEBUG & 2
		Log(socket, "sendBuffer:  send OK. sent=%d", sent);
		#endif
	}

	#if DEBUG & 2
	Log(socket, "sendBuffer:  return OK. numBytes=%d", numBytes);
	#endif
	return numBytes;
}

#if USE_SENDFILE == 1
ssize_t sendFile(const int fd, const int socket, const ssize_t count) {
	ssize_t numBytes, remaining, sent;

	numBytes = 0;
	remaining = count;

	while (remaining > 0) {
		#if DEBUG & 2
		Log(socket, "sendFile: loop iteration.");
		#endif

		sent = sendfile(socket, fd, NULL, remaining);
		if (sent == 0) {
			#if DEBUG & 2
			Log(socket, "SsendFileF: pipe strangeness. sent=%d", sent);
			#endif
			return -ESTRANGE; // error
		}
		if (sent < 0 ) {
			#if DEBUG & 2
			Log(socket, "sendFile: pipe error. sent=%d, errno=%d", sent, errno);
			#endif
			//if (errno == EAGAIN)
			//	continue;
			return sent; // propagate error
		}
		numBytes += sent;
		remaining -= sent;
		#if DEBUG & 2
		Log(socket, "sendFile: send OK. sent=%d", sent);
		#endif
	}

	#if DEBUG & 2
	Log(socket, "sendFile: return OK. numBytes=%d", numBytes);
	#endif
	return numBytes;
}
#endif

#if USE_SENDFILE != 1 || defined(PUT_PATH)
ssize_t pipeStream(const int in, const int out, ssize_t count) {
	char buf[16384];
	ssize_t received, sent;
	ssize_t remainingToBeSent;
	ssize_t remainingToBeRead = count;
	ssize_t total = 0;

	#if DEBUG & 2
	Log(out, "pipeStream:  entering.");
	#endif
	while (count == 0 || remainingToBeRead > 0) {
		received = read(in, buf, (count == 0 || remainingToBeRead >= sizeof(buf)) ? sizeof(buf) : remainingToBeRead );
		if (received == 0) {
			#if DEBUG & 2
			Log(out, "pipeStream:  side exit. received=%d, total=%d", received, total);
			#endif
			return total;
		}
		if (received < 0) {
			#if DEBUG & 2
			Log(out, "pipeStream:  read error. received=%d, errno=%d", received, errno);
			#endif
			//if (errno == EAGAIN)
			//	continue;
			return received; // propagate error
		}
		remainingToBeSent = received;
		while (remainingToBeSent > 0) {
			sent = sendBuffer(out, buf, remainingToBeSent);
			if (sent < 0) {
				#if DEBUG & 2
				Log(out, "pipeStream:  send error. sent=%d", sent);
				#endif
				//if (errno == EAGAIN)
				//	continue;
				return sent; // propagate error
			}
			remainingToBeSent -= sent;
		}
		total += received;
		if (count > 0)
			remainingToBeRead -= received;
	}
	#if DEBUG & 2
	Log(out, "pipeStream:  main exit. total=%d", total);
	#endif
	return total;
}
#endif
