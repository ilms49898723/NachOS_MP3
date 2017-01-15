#!/bin/bash

if [ $# -eq 1 ] && [ "$1" == "clean" ]; then
    make clean
    cd ../build.linux && make clean
else
    make clean && make
    cd ../build.linux && make clean && make
fi

