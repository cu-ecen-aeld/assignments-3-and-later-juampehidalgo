#!/bin/sh

filesdir=$1
searchstr=$2

if [ $# -ne 2 ]
then
    echo "Missing arguments"
    exit 1
fi

if [ -d "$filesdir" ]
then
    num_files=$(find "$filesdir" -type f | wc -l)
    num_hits=$(grep -r "$filesdir" -e "$searchstr" | wc -l)
    echo "The number of files are $num_files and the number of matching lines are $num_hits"
    exit 0
else
    echo "First parameter does not point to folder"
    exit 1
fi
