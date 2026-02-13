#!/bin/bash
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <Source file>"
    exit 1
fi

clang++ -o0 -static $1 -o verify_fi
