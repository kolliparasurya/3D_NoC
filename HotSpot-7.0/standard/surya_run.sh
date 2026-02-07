#!/bin/bash

FILE_PATH=$1

if [ -z "$FILE_PATH" ]; then
    echo "Error: No path provided."
    echo "Usage: ./run.sh <path>"
    exit 1
fi

cd hybrid_dpso_algorithm
./new.sh ${FILE_PATH}
cd ..

cd onruntime_algorithm
./new.sh ${FILE_PATH}
cd ..

cd pair_algorithm
./new.sh ${FILE_PATH}
cd ..

