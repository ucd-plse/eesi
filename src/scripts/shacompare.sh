#!/bin/bash

if [ "$(sha256sum $1 | cut -d' ' -f1)" != "$(sha256sum $2 | cut -d' ' -f1)" ]; then
    diff $1 $2
    sha256sum $1
    sha256sum $2 
    exit 1
fi
