#!/bin/sh

target_file=$1
content=$2

if [ $# -ne 2 ]
then
    echo "Missing arguments: {$#}"
    exit 1
fi

mkdir -p "$(dirname "$target_file")" && touch "$target_file"
echo "$content" > "$target_file"

if [ -f "$target_file" ]
then
    contents_ok=$(grep "$target_file" -e "$contents" | wc -l)
    if [ $contents_ok -eq 1 ]
    then
        exit 0
    else
        exit 1
    fi
else
    exit 1
fi

