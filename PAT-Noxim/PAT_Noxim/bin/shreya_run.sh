#!/bin/bash

MY_PATH=$1
dx=$2
dy=$3
dz=$4

if [ -z "$MY_PATH" ]; then
    echo "Error: No path provided."
    echo "Usage: ./run.sh <path>"
    exit 1
fi

export LD_LIBRARY_PATH=../../../systemc-3.0.1/lib-linux64:$LD_LIBRARY_PATH

cp ../../../HotSpot-7.0/standard/shreya_algorithm/mapping/test_traffic_basic.txt traffic_basic.txt
cp ../../../HotSpot-7.0/standard/shreya_algorithm/mapping/test_traffic_reliability.txt traffic_reliability.txt
cp ../../../HotSpot-7.0/standard/shreya_algorithm/mapping/test_traffic_criticality.txt traffic_criticality.txt
cp ../../../HotSpot-7.0/standard/shreya_algorithm/mapping/test_traffic_combined.txt traffic_combined.txt

./noxim -dimx ${dx} -dimy ${dy}  -dimz ${dz} -traffic table traffic_basic.txt > output.txt
cp -r results ${MY_PATH}/basic/
cp ../../../HotSpot-7.0/standard/shreya_algorithm/mapping/mapFile_basic.txt ${MY_PATH}/basic/mapFile.txt
cp output.txt ${MY_PATH}/basic/report_values.txt
rm -r results
rm output.txt

./noxim -dimx ${dx} -dimy ${dy}  -dimz ${dz} -traffic table traffic_reliability.txt > output.txt
cp -r results ${MY_PATH}/reliability/
cp ../../../HotSpot-7.0/standard/shreya_algorithm/mapping/mapFile_reliability.txt ${MY_PATH}/reliability/mapFile.txt
cp output.txt ${MY_PATH}/reliability/report_values.txt
rm -r results
rm output.txt

./noxim -dimx ${dx} -dimy ${dy}  -dimz ${dz} -traffic table traffic_criticality.txt > output.txt
cp -r results ${MY_PATH}/criticality/
cp ../../../HotSpot-7.0/standard/shreya_algorithm/mapping/mapFile_criticality.txt ${MY_PATH}/criticality/mapFile.txt
cp output.txt ${MY_PATH}/criticality/report_values.txt
rm -r results
rm output.txt

./noxim -dimx ${dx} -dimy ${dy}  -dimz ${dz} -traffic table traffic_combined.txt > output.txt
cp -r results ${MY_PATH}/combined/
cp ../../../HotSpot-7.0/standard/shreya_algorithm/mapping/mapFile_combined.txt ${MY_PATH}/combined/mapFile.txt
cp output.txt ${MY_PATH}/combined/report_values.txt
rm -r results
rm output.txt

cp ../../../HotSpot-7.0/standard/shreya_algorithm/mapping/criticality_reliability_values.txt ${MY_PATH}/criticality_reliability_values.txt
