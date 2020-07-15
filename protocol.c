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

enum HttpCodeIndex {
	HTTP_200,
	HTTP_201,
	HTTP_202,
	HTTP_204,
	HTTP_300,
	HTTP_301,
	HTTP_302,
	HTTP_304,
	HTTP_400,
	HTTP_401,
	HTTP_403,
	HTTP_404,
	HTTP_500,
	HTTP_501,
	HTTP_502,
	HTTP_503
};

#ifdef SERVER_DOCS
const char *httpFile[] = {
	SERVER_DOCS "/200.html",
	SERVER_DOCS "/201.html",
	SERVER_DOCS "/202.html",
	SERVER_DOCS "/204.html",
	SERVER_DOCS "/300.html",
	SERVER_DOCS "/301.html",
	SERVER_DOCS "/302.html",
	SERVER_DOCS "/304.html",
	SERVER_DOCS "/400.html",
	SERVER_DOCS "/401.html",
	SERVER_DOCS "/403.html",
	SERVER_DOCS "/404.html",
	SERVER_DOCS "/500.html",
	SERVER_DOCS "/501.html",
	SERVER_DOCS "/502.html",
	SERVER_DOCS "/503.html"
};
#endif

const char *httpCodeString[] = {
	"200 OK\r",
	"201 Created\r",
	"202 Accepted\r",
	"204 No Content\r",
	"300 Multiple Choices\r",
	"301 Moved Permanently\r",
	"302 Moved Temporarily\r",
	"304 Not Modified\r",
	"400 Bad Request\r",
	"401 Unauthorized\r",
	"403 Forbidden\r",
	"404 Not Found\r",
	"500 Internal Server Error\r",
	"501 Not Implemented\r",
	"502 Bad Gateway\r",
	"503 Service Unavailable\r"
};

const char *connectionString[] = {
	"Connection: keep-alive\r",
	"Connection: close\r"
};

enum ConnectionState httpRequest(const int socket) {

	enum ConnectionState connectionState = CONNECTION_CLOSE;

	char *headerLine; 
	char *method; 
	char *resource;
	char *protocol;
	char *query; 
	char *connection;
	
	struct stat st;
	
	int statusCode;
	int fd = -1;

	unsigned int contentLength;
	const char *contentType;

	char fileNameBuf[512];
	MemPool fileNamePool = { sizeof(fileNameBuf), 0, fileNameBuf };
	char *fileName = fileNameBuf;

	char generalPurposeBuf[2048];
	MemPool generalPurposeMemPool = { sizeof(generalPurposeBuf), 0, generalPurposeBuf };

	char requestHeaderBuf[2048];
	MemPool requestHeaderMemPool = { sizeof(requestHeaderBuf), 0, requestHeaderBuf };
	char *requestHeader[64];
	StringPool requestHeaderPool = { sizeof(requestHeader), 0, requestHeader, &requestHeaderMemPool };

	char replyHeaderBuf[512];
	MemPool replyHeaderMemPool = { sizeof(replyHeaderBuf), 0, replyHeaderBuf };
	char *replyHeader[16];
	StringPool replyHeaderPool = { sizeof(replyHeader), 0, replyHeader, &replyHeaderMemPool };

	#ifdef CGI_PATH
	char *env[96];
	StringPool envPool = { sizeof(env), 0, env, &generalPurposeMemPool };
	#endif

	#if LOG_LEVEL > 0 || defined(CGI_PATH)
	struct sockaddr_in sa;
	int addressLength = sizeof(struct sockaddr_in);
	getpeername(socket, (struct sockaddr *)&sa, (socklen_t *) &addressLength);
	char client[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &sa.sin_addr, client, INET_ADDRSTRLEN);
	int port = ntohs(sa.sin_port);
	#endif

	// Read request header
	if (receiveHeader(socket, &requestHeaderPool, &generalPurposeMemPool) < 0)
		return CONNECTION_CLOSE; // socket is in undefined state

	// Parse first header line
	headerLine = requestHeader[0];
	method = strsep(&headerLine, " ");
	#ifdef PUT_PATH
	int isUpload = !strcmp(method, "PUT") || !strcmp(method, "POST");
	if (!isUpload && strcmp(method, "GET")) {
	#else
	if (strcmp(method, "GET")) {
	#endif
		#if LOG_LEVEL > 0
		Log(socket, "%15s  501  \"%s\"", client, method);
		#endif
		statusCode = HTTP_501;
		goto _sendError; // unknown method
	}
	if (headerLine == NULL) {
		#if LOG_LEVEL > 0
		Log(socket, "%15s  400  \"%s\"", client, method);
		#endif
		statusCode = HTTP_400;
		goto _sendError; // no resource
	}
	resource = strsep(&headerLine, " ");
	if (headerLine == NULL) {
		#if LOG_LEVEL > 0
		Log(socket, "%15s  400  \"%s  %s\"", client, method, resource);
		#endif
		statusCode = HTTP_400;
		goto _sendError; // no protocol
	}
	protocol = strsep(&headerLine, " ");
	#if LOG_LEVEL > 1
	Log(socket, "%15s  OK   \"%s  %s %s\"", client, method, resource, protocol);
	#endif

	connection = strToLower(stringPoolReadVariable(&requestHeaderPool, "Connection"));
	if (strcmp(protocol, PROTOCOL_HTTP_1_1) == 0) {
		connectionState = CONNECTION_KEEPALIVE;
		if (connection != NULL && strcmp(connection, "close") == 0)
			connectionState = CONNECTION_CLOSE;
	} else if (strcmp(protocol, PROTOCOL_HTTP_1_0) == 0) {
		connectionState = CONNECTION_CLOSE;
		if (connection != NULL && strcmp(connection, "keep-alive") == 0)
			connectionState = CONNECTION_KEEPALIVE;
	} else {
		statusCode = HTTP_501;
		goto _sendError;
	}

	query = resource;
	resource = strsep(&query, "?");
	if ( urlDecode(resource) ) // space-saving hack: decode in-place
		goto _sendError500;
	if ( urlDecode(query) ) // space-saving hack: decode in-place
		goto _sendError500;
		
	#ifdef QUERY_HACK
	char newQuery[512];
	char newResource[512];
	MemPool newResourceMemPool = { sizeof(newResource), 0, newResource };
	
	if ( query != NULL )
		if ( *query != '\0' )
			if ( strncmp(resource, QUERY_HACK, strlen(QUERY_HACK)) == 0 ) {
				if ( 
						fileNameEncode(query, newQuery, sizeof(newQuery)) ||
						memPoolAdd(&newResourceMemPool, resource) || 
						memPoolExtend(&newResourceMemPool, "?") || 
						memPoolExtend(&newResourceMemPool, newQuery) 
					)
					goto _sendError500;
				resource = newResource;
			}
	#endif

	if (strstr(resource, "/..")) {
		#if LOG_LEVEL > 0
		Log(socket, "%15s  403  \"SEC  %s %s\"", client, resource, protocol);
		#endif
		statusCode = HTTP_403;
		goto _sendError; // potential security risk - zero tolerance
	}

	#ifdef CGI_PATH
	if (!strncmp(resource, CGI_PATH, strlen(CGI_PATH))) { // presence of CGI path prefix indicates CGI script
		if ( memPoolAdd(&fileNamePool, CGI_DIR) || memPoolExtend(&fileNamePool, resource + strlen(CGI_PATH)) )
			goto _sendError500;
		if (stat(fileName, &st)) {
			#if LOG_LEVEL > 2
			Log(socket, "%15s  404  \"CGI  %s %s\"", client, fileName, query == NULL ? "" : query);
			#endif
			statusCode = HTTP_404;
			goto _sendError;
		}
		if (!S_ISREG(st.st_mode)) {
			#if LOG_LEVEL > 2
			Log(socket, "%15s  403  \"CGI  %s %s\"", client, fileName, query == NULL ? "" : query);
			#endif
			statusCode = HTTP_403;
			goto _sendError;
		}
		if (access(fileName, X_OK)) {
			#if LOG_LEVEL > 2
			Log(socket, "%15s  503  \"CGI  %s %s\"", client, fileName, query == NULL ? "" : query);
			#endif
			statusCode = HTTP_503;
			goto _sendError;
		}
		#if LOG_LEVEL > 3
		Log(socket, "%15s  000  \"CGI  %s %s\"", client, fileName, query == NULL ? "" : query);
		#endif

		// set up environment of cgi program
		stringPoolReset(&envPool);
		stringPoolReset(&replyHeaderPool);
		if (
			stringPoolAddVariable(&envPool, "SERVER_NAME",  SERVER_NAME) ||
			stringPoolAddVariable(&envPool, "SERVER_PORT",  SERVER_PORT_STR) ||
			stringPoolAddVariable(&envPool, "SERVER_SOFTWARE", SERVER_SOFTWARE) ||
			stringPoolAddVariable(&envPool, "SERVER_PROTOCOL", protocol) ||
			stringPoolAddVariable(&envPool, "REQUEST_METHOD", method) ||
			stringPoolAddVariable(&envPool, "SCRIPT_FILENAME", fileName) ||
			stringPoolAddVariable(&envPool, "QUERY_STRING", (query == NULL) ? "" : query) ||
			stringPoolAddVariable(&envPool, "REMOTE_ADDR", client) ||
			stringPoolAddVariableNumber(&envPool, "REMOTE_PORT", port) ||
			stringPoolAddVariables(&envPool, &requestHeaderPool, "HTTP_") ||
			//set up reply header
			stringPoolAdd(&replyHeaderPool, protocol) ||
			memPoolExtendChar(&replyHeaderMemPool, ' ') ||
			memPoolExtend(&replyHeaderMemPool, httpCodeString[HTTP_200]) ||
			stringPoolAdd(&replyHeaderPool, "Server: " SERVER_SOFTWARE "\r") ||
			stringPoolAdd(&replyHeaderPool, connectionString[CONNECTION_CLOSE])
		) {
			#if LOG_LEVEL > 0
			Log(socket, "Memory Error preparing CGI header for %s", fileName);
			#endif
			goto _sendError500;
		}
		memPoolReplace(&replyHeaderMemPool, '\0', '\n');
				
		pid_t childPid = fork();
		if (childPid == 0) {
			// child process continues here
			// Note: the child path should only be debugged in emergency cases because
			// the logFile mutex will not be released if set at the time of forking
			// and thus a fraction of the forked processes will hang
			#if DEBUG & 512
			Log(socket, "CGI Fork Child: Preparing to launch %s", fileName);
			#endif
			if (sendMemPool(socket, &replyHeaderMemPool) >= 0 ) {
				#if DEBUG & 512
				Log(socket, "CGI Fork Child: Reply header sent for %s", fileName);
				#endif
				if (dup2(socket, 1) == 1) {
					#if DEBUG & 512
					Log(socket, "CGI Fork Child: Executing %s", fileName);
					#endif
					chdir(CGI_DIR);
					execve(fileName, NULL, env); // should never return
				}
				#if DEBUG & 512
				Log(socket, "CGI Fork Child: Failed to launch %s", fileName);
				#endif
				exit(-1); // exit child process
			}
			#if DEBUG & 512
			Log(socket, "CGI Fork Child: Failed to send header for %s", fileName);
			#endif
			exit(-1); // exit child process
		}
		// parent process continues here
		if (childPid == -1)
			goto _sendError500; // fork error
		// unfortunately it turns out to be better to wait for the child to exit
		// since otherwise the connection hangs on rare occasions
		#if DEBUG & 256
		Log(socket, "CGI Fork Parent: waiting for child %d", childPid);
		#endif
		waitpid(childPid, NULL, 0);
		#if DEBUG & 256
		Log(socket, "CGI Fork Parent: wait finished for child %d", childPid);
		#endif
		return CONNECTION_CLOSE;
	}
	#endif

	if ( memPoolAdd(&fileNamePool, DOC_DIR) || memPoolExtend(&fileNamePool, resource) ) 
		goto _sendError500;

	#ifdef PUT_PATH
	if (isUpload) {
		if (strncmp(resource, PUT_PATH, strlen(PUT_PATH))) { // PUT path prefix must be present
			statusCode = HTTP_400;
			goto _sendError;
		}
		// TODO: upload file
		statusCode = HTTP_200;
		goto _sendEmptyResponse;
	}
	#endif

	statusCode = HTTP_404;

	if (stat(fileName, &st)) {
		#if LOG_LEVEL > 2
		Log(socket, "%15s  404  \"STAT %s\"", client, fileName);
		#endif
		goto _sendError;
	}

	#ifdef DEFAULT_INDEX
	if (S_ISDIR(st.st_mode)) {
		#ifdef AUTO_INDEX
		if (listDirectory(&fileNamePool, &generalPurposeMemPool))
			goto _sendError500;
		else
			goto _sendContent200;
		#else
		if (fileName[strlen(fileName) - 1] != '/') {
			// We cannot simply append "/" DEFAULT_INDEX
			// because browsers will misinterpret relative links.
			// Redirecting is difficult due to hex encoding
			// and query string having been lost at this point.
			// So let's just admit defeat:
			goto _sendError;
		}
		if (memPoolExtend(&fileNamePool, DEFAULT_INDEX) != 0)
			goto _sendError500;
		if (stat(fileName, &st) < 0) {
			#if LOG_LEVEL > 2
			Log(socket, "%15s  404  \"INST %s\"", client, fileName);
			#endif
			goto _sendError;
		}
		#endif
	}
	#endif
	
	if (!S_ISREG(st.st_mode)) {
		#if LOG_LEVEL > 2
		Log(socket, "%15s  404  \"REGF %s\"", client, fileName);
		#endif
		goto _sendError;
	}

	if ((fd = open(fileName, O_RDONLY)) < 0) {
		#if LOG_LEVEL > 2
		Log(socket, "%15s  404  \"OPEN %s\"", client, fileName);
		#endif
		goto _sendError;
	}

	#if LOG_LEVEL > 3
	Log(socket, "%15s  000  \"OPEN %s\"", client, fileName);
	#endif

_sendContent200:

	statusCode = HTTP_200;

_sendContent:

	contentLength = fd < 0 ? (unsigned) generalPurposeMemPool.current : (unsigned) st.st_size;
	contentType = fd < 0 ? "application/json" : mimeType(fileName);

	stringPoolReset(&replyHeaderPool);
	if (
		stringPoolAdd(&replyHeaderPool, protocol) ||
		memPoolExtendChar(&replyHeaderMemPool, ' ') ||
		memPoolExtend(&replyHeaderMemPool, httpCodeString[statusCode]) ||
		stringPoolAdd(&replyHeaderPool, "Server: " SERVER_SOFTWARE "\r") ||
		stringPoolAdd(&replyHeaderPool, "Content-Length: ") ||
		memPoolExtendNumber(&replyHeaderMemPool, contentLength) ||
		memPoolExtendChar(&replyHeaderMemPool, '\r') ||
		stringPoolAdd(&replyHeaderPool, "Content-Type: ") ||
		memPoolExtend(&replyHeaderMemPool, contentType) ||
		memPoolExtendChar(&replyHeaderMemPool, '\r') ||
		#ifdef PRAGMA
		stringPoolAdd(&replyHeaderPool, "Pragma: " PRAGMA "\r") ||
		#endif
		stringPoolAdd(&replyHeaderPool, connectionString[connectionState]) ||
		stringPoolAdd(&replyHeaderPool, "\r")
	) {
		#if LOG_LEVEL > 0
		Log(socket, "Memory Error preparing reply header for %s", resource);
		#endif
		statusCode = HTTP_500;
		goto _sendEmptyResponse;
	}
	memPoolReplace(&replyHeaderMemPool, '\0', '\n');
		
	#ifdef TCP_CORK // Linux specific
	int option = 1;
	setsockopt(socket, SOL_TCP, TCP_CORK, &option, sizeof(option));
	#endif
	
	if (sendMemPool(socket, &replyHeaderMemPool) < 0 || (fd < 0 ? sendMemPool(socket, &generalPurposeMemPool) : sendFile(socket, fd, contentLength)) < 0)
		connectionState = CONNECTION_CLOSE;
	
	#ifdef TCP_CORK // Linux specific
	option = 0;
	setsockopt(socket, SOL_TCP, TCP_CORK, &option, sizeof(option));
	#endif

	if (fd >= 0)
		close(fd);

	return connectionState;

_sendError500:

	statusCode = HTTP_500;

_sendError:

	#ifdef SERVER_DOCS
	fileName = (char *)(httpFile[statusCode]);

	if (stat(fileName, &st) < 0) {
		#if LOG_LEVEL > 0
		Log(socket, "Server Doc Stat Error \"STAT %s\"", fileName);
		#endif
		goto _sendEmptyResponse;
	}
	else if ((fd = open(fileName, O_RDONLY)) < 0) {
		#if LOG_LEVEL > 0
		Log(socket, "Server Doc Open Error  \"OPEN %s\"", fileName);
		#endif
		goto _sendEmptyResponse;
	}
	
	goto _sendContent; // send the standard error file
	#endif

	// fall through if SERVER_DOCS is not defined
	
_sendEmptyResponse:

	stringPoolReset(&replyHeaderPool);
	if (
		stringPoolAdd(&replyHeaderPool, protocol) ||
		memPoolExtendChar(&replyHeaderMemPool, ' ') ||
		memPoolExtend(&replyHeaderMemPool, httpCodeString[statusCode]) ||
		stringPoolAdd(&replyHeaderPool, "Server: " SERVER_SOFTWARE "\r") ||
		stringPoolAdd(&replyHeaderPool, "Content-Length: 0\r") ||
		#ifdef PRAGMA
		stringPoolAdd(&replyHeaderPool, "Pragma: " PRAGMA "\r") ||
		#endif
		stringPoolAdd(&replyHeaderPool, connectionString[connectionState]) ||
		stringPoolAdd(&replyHeaderPool, "\r")
	) {
		#if LOG_LEVEL > 0
		Log(socket, "Memory Error preparing empty response %d", statusCode);
		#endif
		return CONNECTION_CLOSE;
	}
	memPoolReplace(&replyHeaderMemPool, '\0', '\n');
		
	return sendMemPool(socket, &replyHeaderMemPool) >= 0 ? connectionState : CONNECTION_CLOSE;

}

