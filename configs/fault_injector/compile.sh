#!/bin/bash
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <Source file>"
    exit 1
fi

filename=$(basename "${1%.*}")
dirname=$(dirname "$1")
clang++ -o0 -static $1 -o ${dirname}/${filename}.bin
