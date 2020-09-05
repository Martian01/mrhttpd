#!/bin/bash

echo
echo "This script deletes a resource from a web server"
echo

echo "Server Url: $SERVER_URL"
echo

if [[ "$1" != "" ]] ; then
	curl -s -f -o /dev/null $SERVER_URL$1
	rc=$?
	if [[ "$rc" = "0" ]] ; then
		curl -s -f -X DELETE $SERVER_URL$1
		rc=$?
		echo "Deleted $SERVER_URL$1 with return code $rc"
		echo
	else
		echo "The target resource does not exist on the server (return code $rc)"
		echo
	fi
else
	echo "Usage:   $0 <absolute server path>"
	echo
	echo "Example: $0 /fs/files/demo"
	echo
	exit 1
fi
