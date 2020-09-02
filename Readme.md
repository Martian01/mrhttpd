# Welcome to mrhttpd

You can read the server name as Mr Httpd if you like, although in reality the name refers to the author's initials.

Mrhttpd is maintained and copyrighted by Martin Rogge. The software is licensed to you under the GNU GENERAL PUBLIC LICENSE, Version 2. The complete wording of the GPL is available in the file [COPYING](COPYING).

Mrhttpd was originally developed at [Sourceforge](http://sourceforge.net/projects/mrhttpd). The repository has been moved over to [GitHub](https://github.com/Martian01/mrhttpd) at some point.

Before we get into more detail, here is the quick installation guide for the impatient reader:

## Quick Local Installation Guide

1. Review `mrhttpd.conf`

2. `./configure`

3. `make`

4. as root: `make install`

NB: you can skip step 2 and 3 if you feel lucky

## Quick Docker Installation Guide

	docker run -d -p 8080:8080 -v <your directory>:/opt/mrhttpd/public dockahdockah/mrhttpd

## Credits

Mrhttpd started as a modification of an existing open source project, called GazTek HTTP Daemon (ghttpd for short). The GazTek HTTP Daemon was copyrighted under the GPL by Gareth Owen (<gaz@athene.co.uk>), last available under http://members.xoom.com/gaztek.

In the meantime mrhttpd has evolved significantly and the source code bears no more resemblance to ghttpd. Also the runtime behaviour shows fast performance and enterprise strength stability.

The documentation you are reading right now has originally been written in plain text and incrementally extended over various mrhttpd releases.

## Concept & Restrictions

Mrhttpd is a threaded web server that is lightning fast, simple, robust, secure and has a very small memory footprint. The binary is 15 to 20 kilobytes in size, depending on configuration and CPU architecture. mrhttpd serves files at 3 to 4 times the throughput of Apache and runs CGI scripts.

Mrhttpd is not designed to implement the full HTTP protocol. Since version 2.0 mrhttpd supports HTTP Keep-Alive. Since version 2.5 mrhttpd supports DELETE requests and PUT requests for simple body payloads. POST requests containing multi-part forms are not supported at this stage.

TLS encryption is not supported. You can put mrhttpd behind a reverse proxy if TLS is required.

Having started as a Linux specific project taking advantage of a few Linux specific featurs, the coding should be portable to other operating systems by now. However, the runtime behaviour on other operating systems is not regularly tested. Your mileage may vary.

Mrhttpd is now also available as Docker image. At 2.7 MB compressed, 5.6 MB uncompressed, it is probably the smallest and fastest web server available as Docker image. Whenever you need to serve static files as part of your project, why not fire up the mrhttpd Docker image.

## Background & History

The reason I started working on mrhttpd was that I needed a web server providing simple services in my local area network at home. In particular, it had to serve static files and run Perl scripts on my internet router which was a self-made and highly specialised Linux system. In fact, I had previously installed Apache web server on the internet router and run it for years. Functionally, I have never had any reason to complain about Apache. The only reason for moving away from Apache is that it was an over-dimensioned solution.

Not knowing much about socket programming in the beginning, I started experimenting with various open source web servers. Whilst many of them were inherently unstable and of bad performance I could learn about the basic structure of a web server. This excercise led to the first usable version of mrhttpd which was rock solid and performing well in my local area network.

Eventually competitiveness got the better of me and I compared benchmark results with Apache. The astounding result was, despite being so much more concise, mrhttpd would only reach half the speed of Apache in serving files. The reason was, mrhttpd would fork a new process for every incoming connection, that is one for every GET request. Apache, on the other hand, relies on a number of already forked worker processes and distribution of work via inter-process communication. So I took up the gauntlet, did some reading about threads and finally transformed the server engine into a fully threaded design. The result was a speed increase by more than factor four when serving static files. mrhttpd was able to outperform the Apache server even when the latter used HTTP Keep-Alive (a feature that mrhttpd offers only since version 2.0).

Note, however, that CGI programs are always started in a separate process (both by Apache and mrhttpd) and they are usually time consuming. I have not implemented other protocols like FastCGI because so far I had no use for it.

## Configuration

Mrhttpd is configured at compile time and only at compile time. In combination with the small size, this makes it suitable for embedded systems.

In order to configure mrhttpd you load the file `mrhttpd.conf` into an editor. The file distributed with mrhttpd contains explanations of the possible settings as part of the commentary.

In general, the configuration file defines pairs of variables and their values separated by a "=" character. The configuration file is included in bash scripts and make files. This means the file must fulfil the syntactical requirements of both applications. For instance, bash requires that there be no spaces around the "=" sign.

Let us now go through the list of variables. Note that many of these variables are optional and will either default to some useful value, or disable the compilation of the associated feature. For details please refer to the sample configuration file.

#### SYSTEM_USER
defines the user account under which mrhttpd will run. Note: If mrhttpd is configured to listen at a non-privileged port it need not be started as root. However, if it is started as root you should specify a user with restricted access rights here.

#### PATH_PREFIX
is a string expected at the beginning of every valid resource path. Any request without this prefix will be rejected. The prefix will be omitted from the path of the file system resource. This function is sometimes used behind URL-based load balancers.

#### SERVER_PORT
defines the TCP port the server is listening on. The default is port 8080, but you can use any other port you like. It is quite possible to run multiple server instances at the same time, each one configured for a different port.

#### SERVER_ROOT
specifies the directory that will become the chroot jail of the server. All other paths with the exception of BIN_DIR are therefore relative to SERVER_ROOT. Be aware that a chroot jail can be very restrictive. In particular all your document files and all binaries and libraries required for running external programs must be replicated in the chroot jail.

#### BIN_DIR
defines the installation directory for the binary. This is the only directory not affected by SERVER_ROOT since it is not runtime relevant.

#### PRIVATE_DIR
defines the root directory for internal files. Internal files are documents like the 404 error page.

#### PUBLIC_DIR
defines the root directory for public files (HTML, CSS, JPG, etc.), ie. the actual productive content of the web server.

#### CGI_DIR
defines the root directory for CGI scripts.

#### CGI_PATH
defines the URL prefix indicating that the resource is a CGI script rather than a static file. Whenever mrhttpd finds this string at the beginning of the resource path, it assumes the resource is an executable CGI script.

#### PUT_PATH
defines the URL prefix and the base directory for uploads. A PUT or DELETE request is accepted and executed if and only if mrhttpd finds this string at the beginning of the resource path.

#### DEFAULT_INDEX
defines the name of the default file in a directory. Typically you use "index.html" or similar. mrhttpd will serve this file if no file name is specified in the URL. For example, if a client requests http://server/path/ mrhttpd will send http://server/path/index.html, if such a file is present at the location "path".

#### AUTO_INDEX
controls whether the server will generate directory listings in JSON format. A default index will take precedence over auto index generation, if present.

#### PRAGMA
specifies an optional Pragma parameter that is sent with every HTTP reply. A typical value is "no-cache" if you want to suppress caching by proxy servers and frontends.

#### LOG_LEVEL
determines the amount of information saved in the log file.

Note: the higher the level the more code is generated. If you specify a log level 0, all routines related to log generation, file handling etc. are omitted from the compilation process.

The meaning of the values is as follows:

 * __LOG_LEVEL=0__ no log entries are created.

 * __LOG_LEVEL=1__ additionally, all rejected requests are logged

 * __LOG_LEVEL=2__ additionally, all accepted requests are logged

 * __LOG_LEVEL=3__ additionally, every unsuccessful reply is logged

 * __LOG_LEVEL=4__ additionally, every successful reply is logged

#### LOG_FILE
defines the file name used for saving the logs. If omitted, all logs are printed to stdout.

#### EXT_FILE_CMD
specifies a binary that is used to determine the mime type of a resource. A typical binary would be the file(1) command. Note that the external file command is called only after evaluating the file suffix according to a list compiled into mrhttpd. The obvious reason is the performance impact of an external call.

The external file command is always called with parameters "-b --mime-type". If you do not want that, you need to specify a shell script stripping the first two parameters.

Note: the external file command and all its dependencies are relative to SERVER_ROOT. If you use SERVER_ROOT you will probably need to copy the following files into the chroot jail (your mileage may vary):

 * usr/bin/file
 * usr/lib/libmagic.so.1
 * usr/lib/libz.so.1
 * lib/ld-linux.so.2
 * lib/libc.so.6
 * etc/file/magic/*

#### USE_SENDFILE
chooses which implementation is used for sending files. The choice is between a routine in user space and the kernel function sendfile(). Whilst the latter promises better performance and lower cpu load, you might want to choose the user space routine for trouble shooting. Also, on operating systems other than Linux you need to select the user space routine.

#### DETACH
controls whether the server sends itself into the background when it starts up. When running the server natively you will almost always want to detach. Inside a Docker container you will not want to detach it.

#### QUERY_HACK
is an option for the processing of query strings. If this variable exists the query string of a resource will be interpreted as part of the file name, provided the resource path begins with the string in QUERY_HACK.

Example: assuming a request for a resource `index.html?sorted=yes`. Normally the server will read the file `index.html`. The query string `sorted=yes` will be omitted. If you specify QUERY_HACK=/, the processing will be different and the server will read the file `index.html?sorted=yes`.

The option is named a hack because it is a violation of the HTTP specification. It is, however, useful when you have mirrored dynamic resources using wget, and wget has created file names containing the query part.

One note about query strings in general: for static file requests they are always ignored (unless QUERY_HACK is specified). They are passed to CGI scripts via the usual mechanism.

## Build

After the configuration step say

	./configure

This will do some basic sanity checks on `mrhttpd.conf` and create the C include `config.h`. Next you may call

	make

and finally

	make install

NB: You need to be root for the latter. As an experienced user you may feel safer to look at the Makefile and do the installation manually. It is not rocket science.

## Starting and Stopping

Mrhttpd is always started without parameters. If it has been configured to detach from the foreground process (option DETACH), it will send itself into the background and the foreground process will exit immediately. In either case you can do a test run from a local web browser by pointing it towards http://localhost:8080/ (or whatever host name, port and resource is appropriate in your case).

Mrhttpd will not read any configuration file at runtime. All parameters have been compiled into the binary. For that reason mrhttpd will accept a SIGHUP signal but it will not do anything.

Mrhttpd can be stopped by sending it the SIGINT or the SIGTERM signal, ie. you can say any of the following:

	kill -2 <pid>
	kill -15 <pid>
	killall -2 mrhttpd
	killall -15 mrhttpd

After receiving those signals, mrhttpd will release the socket and wait 5 seconds for threads and child processes to finish. So you can start another server listening on the same port straight away. Please avoid sending a SIGKILL signal unless there is a good reason.

A typical control script called rc.httpd is provided with mrhttpd. If you want to use it in a standard location like /etc/rc.d/ you need to copy it manually.

	#!/bin/bash
	#
	# Start the web server
	#

	case "$1" in
	  'start')
	     /usr/local/sbin/mrhttpd
	     ;;
	  'stop')
	     killall -2 mrhttpd
	     ;;
	  'restart')
	     killall -2 mrhttpd
	     sleep 1
	     /usr/local/sbin/mrhttpd
	     ;;
	  *)
	     echo "usage: $0 start|stop|restart"
	esac

If your system uses systemd you might want to use a service file similar to the following (untested):

	[Unit]
	Description=mrhttpd
	StartLimitBurst=5
	StartLimitIntervalSec=60

	[Service]
	Type=simple
	User=root
	ExecStart=/usr/local/sbin/mrhttpd
	Restart=always
	RestartSec=5

	[Install]
	WantedBy=multi-user.target

## Performance

Performance testing is a difficult topic. Particularly when you venture into extreme load scenarios. However, in order to detect regressions and ensure stability in normal operations a high load performance test is available.

For release 2.2 I ran the tests on my main development machine which has an Intel Core i3-530 CPU at 3500 MHz and 4GB of RAM. The CPU offers two cores with hyperthreading, resulting in 4 logical processors. The operating system was Slackware 13.37 (plus a few upgrades from Slackware Current). I ran all tests on two kernels, namely on vanilla 3.1.4 and on 3.1.4-ck2, the latter containing the BFS scheduler and the interactivity patches provided by Con Kolivas (http://ck-hack.blogspot.com/).

If you want to repeat my performance tests, please observe that logging has a noticable effect on the results under the extreme work loads created by the test. The results shown here have been obtained with logging disabled for all tested HTTP servers. I have included the test script perftest.pl in the subdirectory extra of the distribution. It requires the benchmark program ab which is part of the Apache distribution.

I have included the raw performance data obtained from the web servers Apache, Lighttpd and mrhttpd in CSV format (comma separated values), as produced by the test script perftest.pl.

As I mentioned in the beginning, performance testing under extreme load is a difficult topic. I found I had to push certain system limits prior to be able to complete the test suite for concurrency levels above 1000 (which for the machine in question is way beyond healthy anyway). In particular I needed to run the following commands beforehand:

	echo 65536 > /proc/sys/net/ipv4/tcp_max_syn_backlog
	echo 65536 > /proc/sys/net/core/somaxconn
	echo 65536 > /proc/sys/net/core/netdev_max_backlog
	echo 16777216 > /proc/sys/net/core/rmem_max
	echo 16777216 > /proc/sys/net/core/wmem_max
	echo 4096 87380 16777216 > /proc/sys/net/ipv4/tcp_rmem
	echo 4096 16384 16777216 > /proc/sys/net/ipv4/tcp_wmem
	echo 16384 > /proc/sys/vm/min_free_kbytes
	ulimit -n 65536
	ulimit -i 65536
	ulimit -u 65536

But even then the kernel interfered when running the performance test for Apache and Lighttpd: the connection was dropped and a system message "Possible SYN flooding" was stored in the system log. From the kernel sources it appears this message is generated when the incoming connections are not accepted quickly enough by the listening server. Be it as it may, I could not find a solution and the corresponding results are therefore missing from the result set.

Looking at the results, it can be said that Apache is trailing behind in virtually all tests. For CGI scripts, Lighttpd seems to be the best performer overall, particularly on the vanilla kernel.

For static HTML pages, particularly when connections are kept alive, mrhttpd wipes the floor with the competition. As an example look at the following result reading 1000000 static files at concurrency level 100:

Kernel Version | Server | Keep-Alive | Requests per s | Rate | Max. response [ms]
---------------|--------|------------|----------------|------|-------------------
3.1.4          | Apache/2.2.21    | yes  |  25427  |  33388  | 265
3.1.4          | lighttpd/1.4.29  | yes  |  54741  |  68892  |   6
3.1.4          | mrhttpd/2.2.0    | yes  |  88898  | 100888  |   5
3.1.4-ck2      | Apache/2.2.21    | yes  |  43760  |  57462  |  17
3.1.4-ck2      | lighttpd/1.4.29  | yes  |  49227  |  61652  |   6
3.1.4-ck2      | mrhttpd/2.2.0    | yes  | 163158  | 185150  |   2

The throughput offered by mrhttpd is three to four times higher than Apache and Lighttpd. Result sets at higher concurrency continue this trend, and at insane concurrency levels like 10000 or 20000 mrhttpd is the only responsive server remaining.

The other stunning fact proven by these figures is the significant impact of the BFS CPU scheduler and other optimizations inherent in the CK patch, leading to almost twice the throughput compared to the vanilla kernel (using the CFQ scheduler). This is even more surprising as the design goal of the CK patch is desktop interactivity rather than server throughput.

It is noticable that a fully threaded design like mrhttpd benefits particularly well from the CK patch, whereas the event-driven design of Lighttpd often seems to perform slightly better under the vanilla kernel.

## Docker

A Dockerfile is provided that can be used to build Docker images. On Docker Hub there is a pre built-image available that has been built using the configuration file `mrhttpd-docker.conf` from the repository. You can employ that Docker image via

	docker run -d -p 8080:8080 -v <document directory>:/opt/mrhttpd/public dockahdockah/mrhttpd

If you want to display your own error pages, you can mount a directory on `/opt/mrhttpd/private`.

There is an alternative Docker image `dockahdockah/mrhttpd-put` built with the configuration file `mrhttpd-docker-put.conf`. It is designed to accept file uploads and file deletions, and provides JSON directory listings. It can be used as a simple HTTP-based file server.

## References

  1. https://github.com/Martian01/mrhttpd
  2. http://sourceforge.net/projects/mrhttpd
  3. http://members.xoom.com/gaztek
  4. http://www.xmlmind.com/
  5. http://ck-hack.blogspot.com/
