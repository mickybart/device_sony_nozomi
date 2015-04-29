#!/bin/bash

AOSP=$1
PATCHES="aosp-patch-md5.lst"

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

# purge
> $PATCHES

# generate md5 file
for file in $(find . -type f -not -name "aosp-patch-*" | sort); do
	if [ -f $AOSP/$file ]; then
		echo "$(md5sum $AOSP/$file | awk '{print $1}') $(md5sum $file | awk '{print $1}') $file" >> $PATCHES
	else
		echo "-------------------------------- $(md5sum $file | awk '{print $1}') $file" >> $PATCHES
	fi
done

