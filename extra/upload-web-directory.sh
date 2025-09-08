#!/bin/bash

echo
echo "This script uploads a directory tree from a local file system to a web server"
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

SRC=${1%%/}/
DST=${2%%/}/

if [[ "$SRC" != "/" ]] && [[ "$DST" != "/" ]] && [[ -d $SRC ]] ; then
	curl -s -f -o /dev/null $SERVER_URL$DST
	rc=$?
	if [[ "$rc" != "0" ]] ; then
		for file in $(find $SRC -type f) ; do
			relative=$(echo $file | sed -e "s#^$SRC##")
			url="$SERVER_URL$DST$relative"
			curl -s -f -X PUT --data-binary "@$file" $url
			rc=$?
			if [[ "$rc" != "0" ]] ; then
				echo "Upload of $file to $url got return code $rc. Aborted."
				echo
				exit 3
			else
				echo "Upload of $file to $url successful."
			fi
		done
		echo
		exit 0
	else
		echo "The target resource already exists on the server. Aborted."
		echo
		exit 4
	fi
else
	echo "Usage:   $0 <file system directory> <absolute server path>"
	echo
	echo "Example: $0 ../Cocos/demo/build/web_mobile /fs/files/demo"
	echo
	exit 5
fi
