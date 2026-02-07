#!/bin/bash

FILE_PATH=$1

if [ -z "$FILE_PATH" ]; then
    echo "Error: No path provided."
    echo "Usage: ./run.sh <path>"
    exit 1
fi

g++ -c -g  new.cpp -o new -I ..
gcc -c -g  ../thermal_simulator.c -o thermal_simulator -I ../../
g++ -g  new thermal_simulator -o new_program -L ../../ -lhotspot -ltinyxml2 -lm
./new_program ${FILE_PATH}
