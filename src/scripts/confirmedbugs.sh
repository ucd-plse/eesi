#!/bin/bash

# Make sure confirmed bugs show up in our results
# Requires the artifact directory to be populated prior to running

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR/../.."

PROJECTS="openssl"

for p in $PROJECTS; do
    BUGS_FILE="results/artifact/${p}-bugs.txt"
    CONFIRMED_FILE="results/camera/${p}-bugs-confirmed.txt"
    for bug in $(cat $CONFIRMED_FILE); do
        if [ -z "$(grep $bug $BUGS_FILE)" ]; then
            echo "$bug not in $BUGS_FILE!"
            exit 2
        fi
    done
done
