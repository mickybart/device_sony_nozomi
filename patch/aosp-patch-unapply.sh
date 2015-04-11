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
	if [ "$hashaosp" == "--------------------------------" ]; then
		if [ -f $AOSP/$filename ]; then
			rm -f $AOSP/$filename || die "failed to remove $AOSP/$filename"
		fi
	else
		cat << EOF | bash
		cd $AOSP/$(dirname "$filename")
		git checkout -- $(basename "$filename") || echo "failed to checkout $filename" >&2
EOF
	fi
done < $PATCHES

while read -r filename
do
	cat << EOF | bash
		mkdir -p $AOSP/$(dirname "$filename")
		cd $AOSP/$(dirname "$filename")
		git checkout -- $(basename "$filename") || echo "failed to checkout $filename" >&2
EOF
done < $DELETE

