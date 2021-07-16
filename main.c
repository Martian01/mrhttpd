/*

mrhttpd v2.7.0
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

#define LISTEN_QUEUE_LENGTH 1024 //sufficient for all tested load scenarios

int masterFd;

int main(void) {
	int rc;
	int newFd;
	struct sockaddr_in localAddress;
	struct passwd *pw;
	pthread_t threadId;

	// Obtain master listen socket
	if ((masterFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		puts("Could not create a socket, exiting");
		exit(1);
	}

	// Allow re-use of port & address
	rc = 1;
	setsockopt(masterFd, SOL_SOCKET, SO_REUSEADDR, (void *) &rc, sizeof(rc));

	localAddress.sin_family = AF_INET;
	localAddress.sin_port = htons(SERVER_PORT);
	localAddress.sin_addr.s_addr = INADDR_ANY;
	memset(&(localAddress.sin_zero), 0, sizeof(localAddress.sin_zero));

	if (bind(masterFd, (struct sockaddr *) &localAddress, sizeof(struct sockaddr)) < 0) {
		puts("Could not bind to port, exiting");
		exit(1);
	}

	if (listen(masterFd, LISTEN_QUEUE_LENGTH) < 0) {
		puts("Could not listen on port, exiting");
		exit(1);
	}

	// Set up signal handlers
	signal(SIGTERM, sigTermHandler);
	signal(SIGINT,  sigIntHandler);
	signal(SIGHUP,  sigHupHandler);
	signal(SIGCHLD, sigChldHandler);

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
	if (!LogOpen(masterFd)) {
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
		Log(masterFd, "Accept");
		#endif
		newFd = accept(masterFd, NULL, NULL);
		#if DEBUG & 4
		Log(masterFd, "New connection for socket %d", newFd);
		#endif
		if (newFd >= 0) {
			// Spawn thread to handle new socket
			// The cast is a non-portable kludge to implement call-by-value
			if (pthread_create(&threadId, NULL, serverThread, (void *) (long) newFd)) {
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
				close(newFd);
				#if DEBUG & 128
				Log(masterFd, "Creation of worker thread failed for socket %d", newFd);
				#endif
			}
		}
	}
}

void *serverThread(void *arg) {
	// Detach thread - it will terminate on its own
	pthread_detach(pthread_self());

	const int socket = (int) (long) arg; // Non-portable kludge to implement call-by-value;

	#if DEBUG & 1
	Log(socket, "Worker thread starting for socket %d", socket);
	#endif
	
	setTimeout(socket);

	while (httpRequest(socket) == CONNECTION_KEEPALIVE)
		;

	// Do not shut down the socket as this will affect running cgi programs.
	// Just close the file descriptor.
	close(socket);
	
	#if DEBUG & 1
	Log(socket, "Worker thread finished for socket %d", socket);
	#endif
	
	return NULL;
}

void reaper() {
	// Clean up zombies
	while(waitpid(-1, NULL, WNOHANG) > 0)
		;
}

int shuttingDown = 0;

void shutDownServer() {
	if (!shuttingDown++) { // should be a mutex but hey
		// Exit fairly gracefully
		close(masterFd);
		sleep(5); // Give threads a chance
		#if (LOG_LEVEL > 0) || (DEBUG > 0)
		LogClose(masterFd);
		#endif
		reaper();
		exit(0);
	}
}

void sigTermHandler(const int signal) {
	#if (LOG_LEVEL > 0) || (DEBUG > 0)
	Log(masterFd, "Terminating...");
	#endif
	shutDownServer();
}

void sigIntHandler(const int signal) {
	#if (LOG_LEVEL > 0) || (DEBUG > 0)
	Log(masterFd, "Interrupt");
	#endif
	shutDownServer();
}

void sigHupHandler(const int signal) {
	#if (LOG_LEVEL > 0) || (DEBUG > 0)
	Log(masterFd, "Hangup");
	#endif
	reaper();
}

void sigChldHandler(const int signal) {
	reaper();
}

