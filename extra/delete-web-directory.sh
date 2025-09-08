#!/bin/bash

echo
echo "This script deletes a resource from a web server"
echo

echo "Server Url: $SERVER_URL"
echo

if [[ "$SERVER_URL" == "" ]] ; then
	echo "The environment variable SERVER_URL is not set. Aborted."
	echo
	exit 1
fi
curl -s -f -o /dev/null $SERVER_URL/
rc=$?
if [[ "$rc" != "0" ]] ; then
	echo "The server seems not to respond. Aborted."
	echo
	exit 2
fi
echo "The server seems to respond. Continuing."
echo

if [[ "$1" != "" ]] ; then
	curl -s -f -o /dev/null $SERVER_URL$1
	rc=$?
	if [[ "$rc" = "0" ]] ; then
		curl -s -f -X DELETE $SERVER_URL$1
		rc=$?
		echo "Target $SERVER_URL$1 deleted (return code $rc)"
		echo
	else
		echo "Target $SERVER_URL$1 not found (return code $rc)"
		echo
	fi
else
	echo "Usage:   $0 <absolute server path>"
	echo
	echo "Example: $0 /fs/files/demo"
	echo
	exit 3
fi
