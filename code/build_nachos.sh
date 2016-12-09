#!/bin/bash

if [ $# -eq 1 ] && [ "$1" == "clean" ]; then
    cd ./build.linux && make clean
else
    cd ./build.linux && make clean && make
fi

