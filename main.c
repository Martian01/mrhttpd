/*

mrhttpd v2.4.4
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

#define LISTEN_QUEUE_LENGTH 1024 //sufficient for all tested load scenarios

int master_fd;

int main(void) {
	int rc;
	int new_fd;
	struct sockaddr_in local_addr;
	struct passwd *pw;
	pthread_t thread_id;

	// Obtain master listen socket
	if ((master_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		puts("Could not create a socket, exiting");
		exit(1);
	}

	// Allow re-use of port & address
	rc = 1;
	setsockopt(master_fd, SOL_SOCKET, SO_REUSEADDR, (void *) &rc, sizeof(rc));

	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(SERVER_PORT);
	local_addr.sin_addr.s_addr = INADDR_ANY;
	memset(&(local_addr.sin_zero), 0, sizeof(local_addr.sin_zero));

	if (bind(master_fd, (struct sockaddr *) &local_addr, sizeof(struct sockaddr)) < 0) {
		puts("Could not bind to port, exiting");
		exit(1);
	}

	if (listen(master_fd, LISTEN_QUEUE_LENGTH) < 0) {
		puts("Could not listen on port, exiting");
		exit(1);
	}

	// Set up signal handlers
	signal(SIGTERM, sigterm_handler);
	signal(SIGINT,  sigterm_handler);
	signal(SIGHUP,  sigchld_handler);
	signal(SIGCHLD, sigchld_handler);

	// Ignore SIGPIPE which can be thrown by sendfile()
	signal(SIGPIPE, SIG_IGN);

	#ifdef SYSTEM_USER
	// Save the user data before the chroot call
	pw = getpwnam(SYSTEM_USER);
	if (pw == NULL) {
		puts("Could not find system user, exiting");
		exit(1);
	}
	#endif

	#ifdef SERVER_ROOT
	// Change directory prior to chroot
	if (chdir(SERVER_ROOT) != 0) {
		puts("Could not change directory, exiting");
		exit(1);
	}

	// Change root directory
 	if (chroot(SERVER_ROOT) != 0) {
		puts("Could not change root directory, exiting");
		exit(1);
	}
	#endif

	#if DETACH == 1
	// Drop into background
	rc = fork();
	if (rc != 0) {
		// Parent process continues here
		if (rc == -1) {
			puts("Could not fork into background, exiting");
			exit(1);
		}
		exit(0);
	}
	// Child process continues here
	#endif
	
	#if (LOG_LEVEL > 0) || (DEBUG > 0)
	if (!Log_open(master_fd)) {
		puts("Could not open log file, exiting");
		exit(1);
	}
	#endif

	fclose(stderr);
	fclose(stdin);

	#ifdef SYSTEM_USER
	// Fall back to desired user account
	setgid(pw->pw_gid);
	setuid(pw->pw_uid);
	#endif

	// Server loop - exit only via signal handler
	for (;;) {
		#if DEBUG & 4
		Log(master_fd, "Debug: start of accept.");
		#endif
		new_fd = accept(master_fd, NULL, NULL);
		#if DEBUG & 4
		Log(master_fd, "Debug: return from accept.");
		#endif
		if (new_fd >= 0) {
			// Spawn thread to handle new socket
			// The cast is a non-portable kludge to implement call-by-value
			if (pthread_create(&thread_id, NULL, serverthread, (void *) (long) new_fd)) {
				// Creation of thread failed - 
				// most likely due to thread overload.
				//
				// We could try to sleep, or to handle
				// the connection in the main thread.
				// But what the heck, in an overload
				// situation we have a problem anyway,
				// so let's just close the socket and
				// get crunching on the next request.
				//
				close(new_fd);
				#if DEBUG & 128
				Log(master_fd, "Debug: creation of worker thread failed. Socket = %d", new_fd);
				#endif
			}
		}
	}
}

void *serverthread(void *arg) {
	// Detach thread - it will terminate on its own
	pthread_detach(pthread_self());

	const int sockfd = (int) (long) arg; // Non-portable kludge to implement call-by-value;

	#if DEBUG & 1
	Log(sockfd, "Debug: worker thread starting.");
	#endif
	
	set_timeout(sockfd);

	while (http_request(sockfd) == CONNECTION_KEEPALIVE)
		;

	// Do not shut down the socket as this will affect running cgi programs.
	// Just close the file descriptor.
	close(sockfd);
	
	#if DEBUG & 1
	Log(sockfd, "Debug: worker thread finished.");
	#endif
	
	return NULL;
}

void sigterm_handler(const int signal) {
	// Exit fairly gracefully
	close(master_fd);
	sleep(5); // Give threads a chance

	#if (LOG_LEVEL > 0) || (DEBUG > 0)
	Log_close(master_fd);
	#endif

	while(waitpid(-1, NULL, WNOHANG) > 0) // Clean up zombies
		;
	exit(0);
}

void sigchld_handler(const int signal) {
	while(waitpid(-1, NULL, WNOHANG) > 0) // Clean up zombies
		;
}

