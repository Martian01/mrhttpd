/*

MrHTTPD v2.2.0
Copyright (c) 2007-2011  Martin Rogge <martin_rogge@users.sourceforge.net>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

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

const char *http_codelist[] =
{
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
const char *http_filelist[] =
{
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

#define error(x) { si = HTTP_ ## x ; goto senderror; }

enum connection_state http_request(const int sockfd)
{
	char filenamebuf[256];
	memdescr filenamedescr = { sizeof(filenamebuf), 0, filenamebuf };

	char *hp; 
	char *method; 
	char *resource;
	char *protocol;
	char *query; 
	char *filename = filenamebuf;
	char *connection;
//	char *location = NULL;
	
	struct stat st;
	
	int si;
	int fd;
	enum connection_state connection_state = CONNECTION_CLOSE;

	char requestheaderbuf[2048];
	memdescr requestheadermempool = { sizeof(requestheaderbuf), 0, requestheaderbuf };
	char *requestheader[41] = { NULL };
	indexdescr requestheaderindex = { 40, 0, requestheader, &requestheadermempool };

	char replyheaderbuf[256];
	memdescr replyheadermempool = { sizeof(replyheaderbuf), 0, replyheaderbuf };
	char *replyheader[11] = { NULL };
	indexdescr replyheaderindex = { 10, 0, replyheader, &replyheadermempool };

	#ifdef CGI_URL
	char envbuf[1024];
	memdescr envmempool = { sizeof(envbuf), 0, envbuf };
	char *env[41] = { NULL };
	indexdescr envindex = { 40, 0, env, &envmempool };
	#endif

	#if LOG_LEVEL > 0 || defined(CGI_URL)
	struct sockaddr_in sa;
	int addrlen = sizeof(struct sockaddr_in);
	getpeername(sockfd, (struct sockaddr *)&sa, &addrlen);
	char client[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &sa.sin_addr, client, INET_ADDRSTRLEN);
	int port = ntohs(sa.sin_port);
	#endif

	// Read request header
	if (recv_header_with_timeout(sockfd, &requestheaderindex) < 0)
		return CONNECTION_CLOSE;

	// Identify first token.
	hp = requestheader[0];
	method = strsep(&hp, " ");
	if (strcmp(method, "GET")) {
		#if LOG_LEVEL > 0
		Log(sockfd, "%15s  501  \"%s\"", client, method);
		#endif
		error(501); // method other than GET
	}
	if (hp == NULL) {
		#if LOG_LEVEL > 0
		Log(sockfd, "%15s  400  \"%s\"", client, method);
		#endif
		error(400); // no resource
	}
	resource = strsep(&hp, " ");
	if (hp == NULL) {
		#if LOG_LEVEL > 0
		Log(sockfd, "%15s  400  \"%s  %s\"", client, method, resource);
		#endif
		error(400); // no protocol
	}
	protocol = strsep(&hp, " ");
	#if LOG_LEVEL > 1
	Log(sockfd, "%15s  OK   \"%s  %s %s\"", client, method, resource, protocol);
	#endif

	connection = str_tolower(id_read_variable(&requestheaderindex, "Connection"));
	if (strcmp(protocol, PROTOCOL_HTTP_1_1) == 0) {
		connection_state = CONNECTION_KEEPALIVE;
		if (connection != NULL && strcmp(connection, "close") == 0)
			connection_state = CONNECTION_CLOSE;
	}
	else if (strcmp(protocol, PROTOCOL_HTTP_1_0) == 0) {
		connection_state = CONNECTION_CLOSE;
		if (connection != NULL && strcmp(connection, "keep-alive") == 0)
			connection_state = CONNECTION_KEEPALIVE;
	}
	else {
		error(501);
	}

	query = resource;
	resource = strsep(&query, "?");
	if ( url_decode(resource, resource, 256) ) // space-saving hack: decode in-place
		error(500);
	if ( url_decode(query, query, 2048) ) // space-saving hack: decode in-place
		error(500);
		
	#ifdef QUERY_HACK
	char newquery[256];
	char newresource[256];
	memdescr newresourcedescr = { sizeof(newresource), 0, newresource };
	
	if ( query != NULL )
		if ( *query != '\0' )
			if ( strncmp(resource, QUERY_HACK, strlen(QUERY_HACK)) == 0 ) {
				if ( 
						filename_encode(query, newquery, sizeof(newquery)) ||
						md_init(&newresourcedescr, resource) || 
						md_extend(&newresourcedescr, "?") || 
						md_extend(&newresourcedescr, newquery) 
					)
					error(500);
				resource = newresource;
			}
	#endif


	if (strstr(resource, "/..")) {
		#if LOG_LEVEL > 0
		Log(sockfd, "%15s  403  \"SEC  %s %s\"", client, resource, protocol);
		#endif
		error(403); // potential security risk - zero tolerance
	}

	#ifdef CGI_URL
	if (!strncmp(resource, CGI_URL, strlen(CGI_URL))) { // presence of CGI URL indicates CGI script
		if ( md_init(&filenamedescr, CGI_DIR) || md_extend(&filenamedescr, resource+strlen(CGI_URL)) )
			error(500);
		if (stat(filename, &st)) {
			#if LOG_LEVEL > 2
			Log(sockfd, "%15s  404  \"CGI  %s %s\"", client, filename ,query==NULL?"":query);
			#endif
			error(404);
		}
		if (!S_ISREG(st.st_mode)) {
			#if LOG_LEVEL > 2
			Log(sockfd, "%15s  403  \"CGI  %s %s\"", client, filename ,query==NULL?"":query);
			#endif
			error(403);
		}
		if (access(filename, 1)) {
			#if LOG_LEVEL > 2
			Log(sockfd, "%15s  503  \"CGI  %s %s\"", client, filename ,query==NULL?"":query);
			#endif
			error(503);
		}
		#if LOG_LEVEL > 3
		Log(sockfd, "%15s  200  \"CGI  %s %s\"", client, filename ,query==NULL?"":query);
		#endif
		pid_t child_pid = fork();
		if (child_pid == 0) {
			// child process continues here
			// Note: the child path should only be debugged in emergency cases because
			// the logfile mutex will not be released if set at the time of forking
			// and thus a fraction of the forked processes will hang
			#if DEBUG & 512
			Log(sockfd, "CGI Fork Child: Preparing to launch %s", filename);
			#endif
			// set up environment of cgi program
			id_add_env_string(&envindex, "SERVER_NAME",  SERVER_NAME);
			id_add_env_string(&envindex, "SERVER_PORT",  SERVER_PORT_STR);
			id_add_env_string(&envindex, "SERVER_SOFTWARE", SERVER_SOFTWARE);
			id_add_env_string(&envindex, "SERVER_PROTOCOL", protocol);
			id_add_env_string(&envindex, "REQUEST_METHOD", method);
			id_add_env_string(&envindex, "SCRIPT_FILENAME", filename);
			id_add_env_string(&envindex, "QUERY_STRING", (query == NULL)?"":query);
			id_add_env_string(&envindex, "REMOTE_ADDR", client);
			id_add_env_number(&envindex, "REMOTE_PORT", port);
			id_add_env_http_variables(&envindex, &requestheaderindex);
			//set up reply header
			id_add_string(&replyheaderindex, protocol);
			md_extend_char(&replyheadermempool, ' ');
			md_extend(&replyheadermempool, http_codelist[HTTP_200]);
			md_extend_char(&replyheadermempool, '\r');
			id_add_string(&replyheaderindex, "Server: " SERVER_SOFTWARE "\r");
			id_add_string(&replyheaderindex, "Connection: close\r");
			md_translate(&replyheadermempool, '\0', '\n');
				
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
			}
			#if DEBUG & 512
			Log(sockfd, "CGI Fork Child: Failed to launch %s", filename);
			#endif
			exit(-1); // some error, exit child process
		}
		// parent process continues here
		if (child_pid == -1)
			error(500); // fork error
		// unfortunately it turns out it is better to wait for the child to exit
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

	if ( md_init(&filenamedescr, DOC_DIR) || md_extend(&filenamedescr, resource) ) 
		error(500);

	if (stat(filename, &st)) {
		#if LOG_LEVEL > 2
		Log(sockfd, "%15s  404  \"STAT %s\"", client, filename);
		#endif
		error(404);
	}
	
	#ifdef DEFAULT_INDEX
	if (S_ISDIR(st.st_mode)) {
		if(filename[strlen(filename) - 1] != '/') {
			// We cannot simply append "/" DEFAULT_INDEX
			// because browsers will misinterpret relative links.
			// Redirecting is difficult due to hex encoding
			// and query string having been lost at this point.
			// So let's just admit defeat.
			error(404);
		}
		if (md_extend(&filenamedescr, DEFAULT_INDEX) != 0)
			error(500);
		if (stat(filename, &st) < 0) {
			#if LOG_LEVEL > 2
			Log(sockfd, "%15s  404  \"INST %s\"", client, filename);
			#endif
			error(404);
		}
	}
	#endif
	
	if (!S_ISREG(st.st_mode)) {
		#if LOG_LEVEL > 2
		Log(sockfd, "%15s  404  \"REGF %s\"", client, filename);
		#endif
		error(404);
	}

	if ((fd = open(filename, O_RDONLY)) < 0) {
		#if LOG_LEVEL > 2
		Log(sockfd, "%15s  404  \"OPEN %s\"", client, filename);
		#endif
		error(404);
	}

	#if LOG_LEVEL > 3
	Log(sockfd, "%15s  200  \"OPEN %s\"", client, filename);
	#endif
	si = HTTP_200;

sendfile:

	id_add_string(&replyheaderindex, protocol);
	md_extend_char(&replyheadermempool, ' ');
	md_extend(&replyheadermempool, http_codelist[si]);
	md_extend_char(&replyheadermempool, '\r');
	id_add_string(&replyheaderindex, "Server: " SERVER_SOFTWARE "\r");
	id_add_string(&replyheaderindex, "Content-Length: ");
	md_extend_number(&replyheadermempool, (unsigned) st.st_size);
	md_extend_char(&replyheadermempool, '\r');
	id_add_string(&replyheaderindex, "Content-Type: ");
	md_extend(&replyheadermempool, mimetype(filename));
	md_extend_char(&replyheadermempool, '\r');
//	if (location) {
//		id_add_string(&replyheaderindex, "Location: ");
//		md_extend(&replyheadermempool, location);
//		md_extend_char(&replyheadermempool, '\r');
//	}
	#ifdef PRAGMA
	id_add_string(&replyheaderindex, "Pragma: " PRAGMA "\r");
	#endif
	id_add_string(&replyheaderindex, (connection_state == CONNECTION_KEEPALIVE) ? "Connection: Keep-Alive\r" : "Connection: close\r");
	id_add_string(&replyheaderindex, "\r");
	md_translate(&replyheadermempool, '\0', '\n');
		
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

senderror:

	#ifdef SERVER_DOCS
	filename = (char *)(http_filelist[si]);

	if (stat(filename, &st) < 0) {
		#if LOG_LEVEL > 0
		Log(sockfd, "Server Doc Stat Error \"STAT %s\"", filename);
		#endif
		si = HTTP_500;
		goto sendfatal;
	}
	else if ((fd = open(filename, O_RDONLY)) < 0) {
		#if LOG_LEVEL > 0
		Log(sockfd, "Server Doc Open Error  \"OPEN %s\"", filename);
		#endif
		si = HTTP_500;
		goto sendfatal;
	}
	
	goto sendfile; // send the standard error file
	#endif
	
sendfatal:

	id_add_string(&replyheaderindex, protocol);
	md_extend_char(&replyheadermempool, ' ');
	md_extend(&replyheadermempool, http_codelist[si]);
	md_extend_char(&replyheadermempool, '\r');
	id_add_string(&replyheaderindex, "Server: " SERVER_SOFTWARE "\r");
	id_add_string(&replyheaderindex, "Content-Length: 0\r");
//	if (location) {
//		id_add_string(&replyheaderindex, "Location: ");
//		md_extend(&replyheadermempool, location);
//		md_extend_char(&replyheadermempool, '\r');
//	}
	#ifdef PRAGMA
	id_add_string(&replyheaderindex, "Pragma: " PRAGMA "\r");
	#endif
	id_add_string(&replyheaderindex, (connection_state == CONNECTION_KEEPALIVE) ? "Connection: Keep-Alive\r" : "Connection: close\r");
	id_add_string(&replyheaderindex, "\r");
	md_translate(&replyheadermempool, '\0', '\n');
		
	send_with_timeout(sockfd, replyheadermempool.mem, replyheadermempool.current);

	return connection_state;

}

