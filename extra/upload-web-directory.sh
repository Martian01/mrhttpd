#!/bin/bash

echo
echo "This script uploads a directory tree from a local file system to a web server"
echo

echo "Server Url: $SERVER_URL"
echo

if [[ "$1" != "" ]] && [[ "$2" != "" ]] && [[ -d $1 ]] ; then
	curl -s -f -o /dev/null $SERVER_URL$1
	rc=$?
	if [[ "$rc" != "0" ]] ; then
		for file in $(find $1 -type f) ; do
			relative=$(echo $file | sed -e "s#^$1##")
			url="$SERVER_URL$2$relative"
			curl -s -f -X PUT --data-binary "@$file" $url
			rc=$?
			if [[ "$rc" != "0" ]] ; then
				echo "Upload of $file to $url got return code $rc. Aborted."
				echo
				exit 3
			fi
		done
	else
		echo "The target resource already exists on the server. Aborted."
		echo
		exit 2
	fi
else
	echo "Usage:   $0 <file system directory> <absolute server path>"
	echo
	echo "Example: $0 ../Cocos/demo/build/web_mobile /fs/files/demo"
	echo
	exit 1
fi
