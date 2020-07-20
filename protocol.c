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

#ifdef PRIVATE_DIR
const char *httpFile[] = {
	PRIVATE_DIR "/200.html",
	PRIVATE_DIR "/201.html",
	PRIVATE_DIR "/202.html",
	PRIVATE_DIR "/204.html",
	PRIVATE_DIR "/300.html",
	PRIVATE_DIR "/301.html",
	PRIVATE_DIR "/302.html",
	PRIVATE_DIR "/304.html",
	PRIVATE_DIR "/400.html",
	PRIVATE_DIR "/401.html",
	PRIVATE_DIR "/403.html",
	PRIVATE_DIR "/404.html",
	PRIVATE_DIR "/500.html",
	PRIVATE_DIR "/501.html",
	PRIVATE_DIR "/502.html",
	PRIVATE_DIR "/503.html"
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
	
	int statusCode;

	struct stat st;
	FILE *file = NULL;
	int fd = -1;

	unsigned int contentLength;
	const char *contentType;

	char fileNameBuf[512];
	MemPool fileNamePool = { sizeof(fileNameBuf), 0, fileNameBuf };
	char *fileName = fileNameBuf;

	char streamBuf[2048];
	MemPool streamMemPool = { sizeof(streamBuf), 0, streamBuf };

	char requestHeaderBuf[2048];
	MemPool requestHeaderMemPool = { sizeof(requestHeaderBuf), 0, requestHeaderBuf };
	char *requestHeader[64];
	StringPool requestHeaderPool = { sizeof(requestHeader), 0, requestHeader, &requestHeaderMemPool };

	char replyHeaderBuf[512];
	MemPool replyHeaderMemPool = { sizeof(replyHeaderBuf), 0, replyHeaderBuf };
	char *replyHeader[16];
	StringPool replyHeaderPool = { sizeof(replyHeader), 0, replyHeader, &replyHeaderMemPool };

	#if LOG_LEVEL > 0 || defined(CGI_PATH)
	struct sockaddr_in sa;
	int addressLength = sizeof(struct sockaddr_in);
	getpeername(socket, (struct sockaddr *)&sa, (socklen_t *) &addressLength);
	char client[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &sa.sin_addr, client, INET_ADDRSTRLEN);
	int port = ntohs(sa.sin_port);
	#endif

	// Read request header
	if (parseHeader(socket, &streamMemPool, &requestHeaderPool) < 0)
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
	if (urlDecode(resource)) // space-saving hack: decode in-place
		goto _sendError500;
	if (urlDecode(query)) // space-saving hack: decode in-place
		goto _sendError500;
		
	#ifdef QUERY_HACK
	char newQuery[512];
	char newResource[512];
	MemPool newResourceMemPool = { sizeof(newResource), 0, newResource };
	
	if (query != NULL)
		if (*query != '\0')
			if (strncmp(resource, QUERY_HACK, strlen(QUERY_HACK)) == 0) {
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
	char *env[96];
	StringPool envPool = { sizeof(env), 0, env, &streamMemPool };

	if (!strncmp(resource, CGI_PATH, strlen(CGI_PATH))) { // presence of CGI path prefix indicates CGI script
		if (memPoolAdd(&fileNamePool, CGI_DIR) || memPoolExtend(&fileNamePool, resource + strlen(CGI_PATH)))
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
				if (dup2(socket, 1) == 1) { // Known Bug: the overspill from parseHeader() is missing from the stream
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

	if (memPoolAdd(&fileNamePool, PUBLIC_DIR)) 
		goto _sendError500;

	#ifdef PUT_PATH
	if (isUpload) {
		if (strncmp(resource, PUT_PATH, strlen(PUT_PATH))) { // PUT path prefix must be present
			#if LOG_LEVEL > 2
			Log(socket, "%15s  403  \"PUT wrong path\"", client);
			#endif
			statusCode = HTTP_403;
			goto _sendError;
		}
		char *headerContentLength = stringPoolReadVariable(&requestHeaderPool, "Content-Length");
		contentLength = headerContentLength == NULL ? 0 : atoi(headerContentLength);
		char *boundary = NULL;
		char* headerContentType = stringPoolReadVariable(&requestHeaderPool, "Content-Type");
		contentType = strsep(&headerContentType, ";");
		if (contentType != NULL && headerContentType != NULL && !strcmp(contentType, "multipart/form-data")) {
			headerContentType = startOf(headerContentType);
			if (!strncmp(headerContentType, "boundary=", 9))
				boundary = headerContentType + 9;
		}
		if (boundary != NULL && *boundary != '\0') {
		// Multi Part Form Request
			#if DEBUG & 1024
			Log(socket, "found multi part form request with boundary \"%s\"", boundary);
			#endif
			#if LOG_LEVEL > 0
			Log(socket, "%15s  400  \"%s  %s\"", client, method, resource);
			#endif
			statusCode = HTTP_400;
			goto _sendError; // multi part form request is not supported at this stage
		}
		// Simple Body Upload
		int uploadFile = openFileForWriting(&fileNamePool, resource);
		if (uploadFile < 0) {
			#if LOG_LEVEL > 2
			Log(socket, "%15s  500  \"PUT file error %d\"", client, uploadFile);
			#endif
			goto _sendError500;
		}
		#if DEBUG & 1024
		Log(socket, "contentLength=\"%u\", overspill=\"%d\"", contentLength, streamMemPool.current);
		#endif
		if (contentLength > 0 && streamMemPool.current > 0) { // overspill from parseHeader()
			unsigned size = contentLength;
			if (streamMemPool.current < size)
				size = streamMemPool.current;
			if (write(uploadFile, streamMemPool.mem, size) != size) {
				#if LOG_LEVEL > 2
				Log(socket, "%15s  500  \"PUT overspill error\"", client);
				#endif
				close(uploadFile);
				goto _sendError500;
			}
			contentLength -= size;
		}
		#if DEBUG & 1024
		Log(socket, "remaining=\"%u\"", contentLength);
		#endif
		if (contentLength > 0 && pipeToFile(socket, uploadFile, contentLength) < 0) {
			#if LOG_LEVEL > 2
			Log(socket, "%15s  500  \"PUT pipe error\"", client);
			#endif
			close(uploadFile);
			goto _sendError500;
		}
		if (close(uploadFile) < 0) {
			#if LOG_LEVEL > 2
			Log(socket, "%15s  500  \"PUT close error\"", client);
			#endif
			goto _sendError500;
		}
		statusCode = HTTP_200;
		goto _sendEmptyResponse;
	}
	#endif

	if (memPoolExtend(&fileNamePool, resource)) 
		goto _sendError500;

	statusCode = HTTP_404;

	if (stat(fileName, &st)) {
		#if LOG_LEVEL > 2
		Log(socket, "%15s  404  \"STAT %s\"", client, fileName);
		#endif
		goto _sendError;
	}

	if (S_ISDIR(st.st_mode)) {
		// normalize directory name
		if (fileName[strlen(fileName) - 1] != '/') {
			if (memPoolExtendChar(&fileNamePool, '/')) 
				goto _sendError500;
		}
		#ifdef DEFAULT_INDEX
		int savePosition = fileNamePool.current;
		if (memPoolExtend(&fileNamePool, DEFAULT_INDEX) != 0)
			goto _sendError500;
		if (stat(fileName, &st) >= 0)
			goto _foundFile;
		memPoolResetTo(&fileNamePool, savePosition);
		#endif
		#if AUTO_INDEX == 1
		// generate auto index if default index was unsuccessful
		file = tmpfile();
		if (file == NULL) {
			#if LOG_LEVEL > 2
			Log(socket, "%15s  500  \"AUTODIR no tmpfile\"", client);
			#endif
			goto _sendError500;
		}
		if (fileWriteDirectory(file, &fileNamePool)) {
			fclose(file);
			#if LOG_LEVEL > 2
			Log(socket, "%15s  500  \"AUTODIR failed write\"", client);
			#endif
			goto _sendError500;
		}
		fflush(file);
		rewind(file);
		fd = fileno(file);
		if (fd < 0 || fstat(fd, &st)) {
			fclose(file);
			#if LOG_LEVEL > 2
			Log(socket, "%15s  500  \"AUTODIR failed rewind\"", client);
			#endif
			goto _sendError500;
		}
		contentType = "application/json";
		goto _sendFile200;
		#else
		#if LOG_LEVEL > 2
		Log(socket, "%15s  404  \"INST %s\"", client, fileName);
		#endif
		goto _sendError;
		#endif
	}
	
_foundFile:

	if (!S_ISREG(st.st_mode)) {
		#if LOG_LEVEL > 2
		Log(socket, "%15s  404  \"REGF %s\"", client, fileName);
		#endif
		goto _sendError;
	}

	fd = open(fileName, O_RDONLY);

	if (fd < 0) {
		#if LOG_LEVEL > 2
		Log(socket, "%15s  404  \"OPEN %s\"", client, fileName);
		#endif
		goto _sendError;
	}

	contentType = mimeType(fileName);

_sendFile200:

	#if LOG_LEVEL > 3
	Log(socket, "%15s  000  \"OPEN %s\"", client, fileName);
	#endif
	statusCode = HTTP_200;

_sendFile:

	contentLength = (unsigned) st.st_size;
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
	
	if (sendMemPool(socket, &replyHeaderMemPool) < 0 || sendFile(socket, fd, contentLength) < 0)
		connectionState = CONNECTION_CLOSE;
	
	#ifdef TCP_CORK // Linux specific
	option = 0;
	setsockopt(socket, SOL_TCP, TCP_CORK, &option, sizeof(option));
	#endif

	if (file != NULL)
		fclose(file);
	else
		close(fd);

	return connectionState;

_sendError500:

	statusCode = HTTP_500;

_sendError:

	connectionState = CONNECTION_CLOSE;

	#ifdef PRIVATE_DIR
	fileName = (char *)(httpFile[statusCode]);
	contentType = "text/html";
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
	
	goto _sendFile; // send the standard error file
	#endif

	// fall through if PRIVATE_DIR is not defined
	
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
