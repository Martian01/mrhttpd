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

enum http_code_index {
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
	HTTP_503,
};

const char *http_code[] = {
	"200 OK",
	"201 Created",
	"202 Accepted",
	"204 No Content",
	"300 Multiple Choices",
	"301 Moved Permanently",
	"302 Moved Temporarily",
	"304 Not Modified",
	"400 Bad Request",
	"401 Unauthorized",
	"403 Forbidden",
	"404 Not Found",
	"500 Internal Server Error",
	"501 Not Implemented",
	"502 Bad Gateway",
	"503 Service Unavailable"
};

#ifdef SERVER_DOCS
const char *http_file[] = {
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

enum connection_state http_request(const int sockfd) {

	char *hp; 
	char *method; 
	char *resource;
	char *protocol;
	char *query; 
	char *connection;
	
	struct stat st;
	
	int statusCode;
	int fd;
	enum connection_state connection_state = CONNECTION_CLOSE;

	char filenamebuf[512];
	mempool filenamepool = { sizeof(filenamebuf), 0, filenamebuf };
	char *filename = filenamebuf;

	char requestheaderbuf[2048];
	mempool requestheadermempool = { sizeof(requestheaderbuf), 0, requestheaderbuf };
	char *requestheader[64];
	stringpool requestheaderpool = { sizeof(requestheader), 0, requestheader, &requestheadermempool };

	char replyheaderbuf[512];
	mempool replyheadermempool = { sizeof(replyheaderbuf), 0, replyheaderbuf };
	char *replyheader[16];
	stringpool replyheaderpool = { sizeof(replyheader), 0, replyheader, &replyheadermempool };

	#ifdef CGI_URL
	char envbuf[3072];
	mempool envmempool = { sizeof(envbuf), 0, envbuf };
	char *env[96];
	stringpool envpool = { sizeof(env), 0, env, &envmempool };
	#endif

	#if LOG_LEVEL > 0 || defined(CGI_URL)
	struct sockaddr_in sa;
	int addrlen = sizeof(struct sockaddr_in);
	getpeername(sockfd, (struct sockaddr *)&sa, (socklen_t *) &addrlen);
	char client[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &sa.sin_addr, client, INET_ADDRSTRLEN);
	int port = ntohs(sa.sin_port);
	#endif

	// Read request header
	if (recv_header_with_timeout(&requestheaderpool, sockfd) < 0)
		return CONNECTION_CLOSE; // socket is in undefined state

	// Identify first token.
	hp = requestheader[0];
	method = strsep(&hp, " ");
	if (strcmp(method, "GET")) {
		#if LOG_LEVEL > 0
		Log(sockfd, "%15s  501  \"%s\"", client, method);
		#endif
		statusCode = HTTP_501;
		goto _senderror; // method other than GET
	}
	if (hp == NULL) {
		#if LOG_LEVEL > 0
		Log(sockfd, "%15s  400  \"%s\"", client, method);
		#endif
		statusCode = HTTP_400;
		goto _senderror; // no resource
	}
	resource = strsep(&hp, " ");
	if (hp == NULL) {
		#if LOG_LEVEL > 0
		Log(sockfd, "%15s  400  \"%s  %s\"", client, method, resource);
		#endif
		statusCode = HTTP_400;
		goto _senderror; // no protocol
	}
	protocol = strsep(&hp, " ");
	#if LOG_LEVEL > 1
	Log(sockfd, "%15s  OK   \"%s  %s %s\"", client, method, resource, protocol);
	#endif

	connection = str_tolower(sp_read_variable(&requestheaderpool, "Connection"));
	if (strcmp(protocol, PROTOCOL_HTTP_1_1) == 0) {
		connection_state = CONNECTION_KEEPALIVE;
		if (connection != NULL && strcmp(connection, "close") == 0)
			connection_state = CONNECTION_CLOSE;
	} else if (strcmp(protocol, PROTOCOL_HTTP_1_0) == 0) {
		connection_state = CONNECTION_CLOSE;
		if (connection != NULL && strcmp(connection, "keep-alive") == 0)
			connection_state = CONNECTION_KEEPALIVE;
	} else {
		statusCode = HTTP_501;
		goto _senderror;
	}

	query = resource;
	resource = strsep(&query, "?");
	if ( url_decode(resource, resource, 512) ) // space-saving hack: decode in-place
		goto _senderror500;
	if ( url_decode(query, query, 2048) ) // space-saving hack: decode in-place
		goto _senderror500;
		
	#ifdef QUERY_HACK
	char newquery[512];
	char newresource[512];
	mempool newresourcemempool = { sizeof(newresource), 0, newresource };
	
	if ( query != NULL )
		if ( *query != '\0' )
			if ( strncmp(resource, QUERY_HACK, strlen(QUERY_HACK)) == 0 ) {
				if ( 
						filename_encode(query, newquery, sizeof(newquery)) ||
						mp_add(&newresourcemempool, resource) || 
						mp_extend(&newresourcemempool, "?") || 
						mp_extend(&newresourcemempool, newquery) 
					)
					goto _senderror500;
				resource = newresource;
			}
	#endif


	if (strstr(resource, "/..")) {
		#if LOG_LEVEL > 0
		Log(sockfd, "%15s  403  \"SEC  %s %s\"", client, resource, protocol);
		#endif
		statusCode = HTTP_403;
		goto _senderror; // potential security risk - zero tolerance
	}

	#ifdef CGI_URL
	if (!strncmp(resource, CGI_URL, strlen(CGI_URL))) { // presence of CGI URL indicates CGI script
		if ( mp_add(&filenamepool, CGI_DIR) || mp_extend(&filenamepool, resource + strlen(CGI_URL)) )
			goto _senderror500;
		if (stat(filename, &st)) {
			#if LOG_LEVEL > 2
			Log(sockfd, "%15s  404  \"CGI  %s %s\"", client, filename, query == NULL ? "" : query);
			#endif
			statusCode = HTTP_404;
			goto _senderror;
		}
		if (!S_ISREG(st.st_mode)) {
			#if LOG_LEVEL > 2
			Log(sockfd, "%15s  403  \"CGI  %s %s\"", client, filename, query == NULL ? "" : query);
			#endif
			statusCode = HTTP_403;
			goto _senderror;
		}
		if (access(filename, 1)) {
			#if LOG_LEVEL > 2
			Log(sockfd, "%15s  503  \"CGI  %s %s\"", client, filename, query == NULL ? "" : query);
			#endif
			statusCode = HTTP_503;
			goto _senderror;
		}
		#if LOG_LEVEL > 3
		Log(sockfd, "%15s  000  \"CGI  %s %s\"", client, filename, query == NULL ? "" : query);
		#endif

		// set up environment of cgi program
		sp_reset(&envpool);
		sp_reset(&replyheaderpool);
		if (
			sp_add_variable(&envpool, "SERVER_NAME",  SERVER_NAME) ||
			sp_add_variable(&envpool, "SERVER_PORT",  SERVER_PORT_STR) ||
			sp_add_variable(&envpool, "SERVER_SOFTWARE", SERVER_SOFTWARE) ||
			sp_add_variable(&envpool, "SERVER_PROTOCOL", protocol) ||
			sp_add_variable(&envpool, "REQUEST_METHOD", method) ||
			sp_add_variable(&envpool, "SCRIPT_FILENAME", filename) ||
			sp_add_variable(&envpool, "QUERY_STRING", (query == NULL) ? "" : query) ||
			sp_add_variable(&envpool, "REMOTE_ADDR", client) ||
			sp_add_variable_number(&envpool, "REMOTE_PORT", port) ||
			sp_add_variables(&envpool, &requestheaderpool, "HTTP_") ||
			//set up reply header
			sp_add(&replyheaderpool, protocol) ||
			mp_extend_char(&replyheadermempool, ' ') ||
			mp_extend(&replyheadermempool, http_code[HTTP_200]) ||
			mp_extend_char(&replyheadermempool, '\r') ||
			sp_add(&replyheaderpool, "Server: " SERVER_SOFTWARE "\r") ||
			sp_add(&replyheaderpool, "Connection: close\r") ||
			mp_replace(&replyheadermempool, '\0', '\n')
		) {
			#if LOG_LEVEL > 0
			Log(sockfd, "Memory Error preparing CGI header for %s", filename);
			#endif
			goto _senderror500;
		}
				
		pid_t child_pid = fork();
		if (child_pid == 0) {
			// child process continues here
			// Note: the child path should only be debugged in emergency cases because
			// the logfile mutex will not be released if set at the time of forking
			// and thus a fraction of the forked processes will hang
			#if DEBUG & 512
			Log(sockfd, "CGI Fork Child: Preparing to launch %s", filename);
			#endif
			if (send_with_timeout(sockfd, replyheadermempool.mem, replyheadermempool.current) >= 0 ) {
				#if DEBUG & 512
				Log(sockfd, "CGI Fork Child: Reply header sent for %s", filename);
				#endif
				if (dup2(sockfd, 1) == 1) {
					#if DEBUG & 512
					Log(sockfd, "CGI Fork Child: Executing %s", filename);
					#endif
					chdir(CGI_DIR);
					execve(filename, NULL, env); // should never return
				}
				#if DEBUG & 512
				Log(sockfd, "CGI Fork Child: Failed to launch %s", filename);
				#endif
				exit(-1); // exit child process
			}
			#if DEBUG & 512
			Log(sockfd, "CGI Fork Child: Failed to send header for %s", filename);
			#endif
			exit(-1); // exit child process
		}
		// parent process continues here
		if (child_pid == -1)
			goto _senderror500; // fork error
		// unfortunately it turns out to be better to wait for the child to exit
		// since otherwise the connection hangs on rare occasions
		#if DEBUG & 256
		Log(sockfd, "CGI Fork Parent: waiting for child %d", child_pid);
		#endif
		waitpid(child_pid, NULL, 0);
		#if DEBUG & 256
		Log(sockfd, "CGI Fork Parent: wait finished for child %d", child_pid);
		#endif
		return CONNECTION_CLOSE;
	}
	#endif

	if ( mp_add(&filenamepool, DOC_DIR) || mp_extend(&filenamepool, resource) ) 
		goto _senderror500;

	statusCode = HTTP_404;

	if (stat(filename, &st)) {
		#if LOG_LEVEL > 2
		Log(sockfd, "%15s  404  \"STAT %s\"", client, filename);
		#endif
		goto _senderror;
	}
	
	#ifdef DEFAULT_INDEX
	if (S_ISDIR(st.st_mode)) {
		if(filename[strlen(filename) - 1] != '/') {
			// We cannot simply append "/" DEFAULT_INDEX
			// because browsers will misinterpret relative links.
			// Redirecting is difficult due to hex encoding
			// and query string having been lost at this point.
			// So let's just admit defeat:
		goto _senderror;
		}
		if (mp_extend(&filenamepool, DEFAULT_INDEX) != 0)
			goto _senderror500;
		if (stat(filename, &st) < 0) {
			#if LOG_LEVEL > 2
			Log(sockfd, "%15s  404  \"INST %s\"", client, filename);
			#endif
		goto _senderror;
		}
	}
	#endif
	
	if (!S_ISREG(st.st_mode)) {
		#if LOG_LEVEL > 2
		Log(sockfd, "%15s  404  \"REGF %s\"", client, filename);
		#endif
		goto _senderror;
	}

	if ((fd = open(filename, O_RDONLY)) < 0) {
		#if LOG_LEVEL > 2
		Log(sockfd, "%15s  404  \"OPEN %s\"", client, filename);
		#endif
		goto _senderror;
	}

	#if LOG_LEVEL > 3
	Log(sockfd, "%15s  000  \"OPEN %s\"", client, filename);
	#endif

	statusCode = HTTP_200;

_sendfile:

	sp_reset(&replyheaderpool);
	if (
		sp_add(&replyheaderpool, protocol) ||
		mp_extend_char(&replyheadermempool, ' ') ||
		mp_extend(&replyheadermempool, http_code[statusCode]) ||
		mp_extend_char(&replyheadermempool, '\r') ||
		sp_add(&replyheaderpool, "Server: " SERVER_SOFTWARE "\r") ||
		sp_add(&replyheaderpool, "Content-Length: ") ||
		mp_extend_number(&replyheadermempool, (unsigned) st.st_size) ||
		mp_extend_char(&replyheadermempool, '\r') ||
		sp_add(&replyheaderpool, "Content-Type: ") ||
		mp_extend(&replyheadermempool, mimetype(filename)) ||
		mp_extend_char(&replyheadermempool, '\r') ||
		#ifdef PRAGMA
		sp_add(&replyheaderpool, "Pragma: " PRAGMA "\r") ||
		#endif
		sp_add(&replyheaderpool, (connection_state == CONNECTION_KEEPALIVE) ? "Connection: Keep-Alive\r" : "Connection: close\r") ||
		sp_add(&replyheaderpool, "\r") ||
		mp_replace(&replyheadermempool, '\0', '\n')
	) {
		#if LOG_LEVEL > 0
		Log(sockfd, "Memory Error preparing reply header for %s", resource);
		#endif
		statusCode = HTTP_500;
		goto _sendfatal;
	}
		
	#ifdef TCP_CORK // Linux specific
	int option = 1;
	setsockopt(sockfd, SOL_TCP, TCP_CORK, &option, sizeof(option));
	#endif
	
	if (send_with_timeout(sockfd, replyheadermempool.mem, replyheadermempool.current) >= 0)
		sendfile_with_timeout(sockfd, fd, st.st_size);
	
	#ifdef TCP_CORK // Linux specific
	option = 0;
	setsockopt(sockfd, SOL_TCP, TCP_CORK, &option, sizeof(option));
	#endif

	close(fd);

	return connection_state;

_senderror500:

	statusCode = HTTP_500;

_senderror:

	#ifdef SERVER_DOCS
	filename = (char *)(http_file[statusCode]);

	if (stat(filename, &st) < 0) {
		#if LOG_LEVEL > 0
		Log(sockfd, "Server Doc Stat Error \"STAT %s\"", filename);
		#endif
		goto _sendfatal;
	}
	else if ((fd = open(filename, O_RDONLY)) < 0) {
		#if LOG_LEVEL > 0
		Log(sockfd, "Server Doc Open Error  \"OPEN %s\"", filename);
		#endif
		goto _sendfatal;
	}
	
	goto _sendfile; // send the standard error file
	#endif

	// fall through if SERVER_DOCS is not defined
	
_sendfatal:

	sp_reset(&replyheaderpool);
	if (
		sp_add(&replyheaderpool, protocol) ||
		mp_extend_char(&replyheadermempool, ' ') ||
		mp_extend(&replyheadermempool, http_code[statusCode]) ||
		mp_extend_char(&replyheadermempool, '\r') ||
		sp_add(&replyheaderpool, "Server: " SERVER_SOFTWARE "\r") ||
		sp_add(&replyheaderpool, "Content-Length: 0\r") ||
		#ifdef PRAGMA
		sp_add(&replyheaderpool, "Pragma: " PRAGMA "\r") ||
		#endif
		sp_add(&replyheaderpool, (connection_state == CONNECTION_KEEPALIVE) ? "Connection: Keep-Alive\r" : "Connection: close\r") ||
		sp_add(&replyheaderpool, "\r") ||
		mp_replace(&replyheadermempool, '\0', '\n')
	) {
		#if LOG_LEVEL > 0
		Log(sockfd, "Memory Error preparing fatal response %d", statusCode);
		#endif
		return CONNECTION_CLOSE;
	}
		
	send_with_timeout(sockfd, replyheadermempool.mem, replyheadermempool.current);

	return connection_state;

}

