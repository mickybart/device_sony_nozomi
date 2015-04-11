#!/bin/bash

AOSP=$1
PATCHES="aosp-patch-md5.lst"
DELETE="aosp-patch-delete.lst"

function die {
        echo "ERROR - ${1-UNKNOWN}" >&2
        exit 1
}

function usage {
        cat << EOF
usage : $0 aosp
 aosp : path to your local aosp repo sync

EOF
        exit 2
}

# parameters
if [ $# -ne 1 ]; then
        usage
fi

[ ! -f $PATCHES ] && die "file $PATCHES not found"

while read -r hashaosp hashnew filename
do
	if [ -f $AOSP/$filename ]; then
		hash=$(md5sum $AOSP/$filename | awk '{print $1}')
	else
		hash="--------------------------------"
	fi

	if [ "$hash" == "$hashaosp" ]; then
		mkdir -p $AOSP/$(dirname "$filename")
		cp -af $filename $AOSP/$filename || die "failed to copy $filename into $AOSP/$filename"
	else
		if [ "$hash" != "$hashnew" ]; then
			die "patch $filename is not valid in this context. please merge manually"
		fi
	fi
done < $PATCHES

while read -r filename
do
	if [ -f $AOSP/$filename ]; then
		rm -f $AOSP/$filename || die "failed to remove $AOSP/$filename"
	fi
done < $DELETE

