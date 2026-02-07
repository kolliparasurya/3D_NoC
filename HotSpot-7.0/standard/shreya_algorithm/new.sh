#!/bin/bash
FILE_PATH=$1
#multimedia_benchmark/VOPD_task_graph.xml"

g++ -c -g  new.cpp -o new -I ..
gcc -c -g  ../thermal_simulator.c -o thermal_simulator -I ../../
g++ -g  new thermal_simulator -o new_program -L ../../ -lhotspot -ltinyxml2 -lm

./new_program ${FILE_PATH} 1
./new_program ${FILE_PATH} 2
./new_program ${FILE_PATH} 3
./new_program ${FILE_PATH} 4

