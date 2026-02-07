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

cp ../../../HotSpot-7.0/standard/hybrid_dpso_algorithm/mapping/test_traffic.txt traffic_hdpso.txt
cp ../../../HotSpot-7.0/standard/onruntime_algorithm/mapping/test_traffic.txt traffic_onruntime.txt
cp ../../../HotSpot-7.0/standard/pair_algorithm/mapping/test_traffic.txt traffic_pair.txt

./noxim -dimx ${dx} -dimy ${dy} -dimz ${dz} -traffic table traffic_hdpso.txt > output.txt
cp -r results ${MY_PATH}/hybrid_dpso_algorithm/
cp ../../../HotSpot-7.0/standard/hybrid_dpso_algorithm/mapping/hybrid_dpso_report_values.txt ${MY_PATH}/hybrid_dpso_algorithm/reference_values.txt
cp output.txt ${MY_PATH}/hybrid_dpso_algorithm/report_values.txt
rm -r results
rm output.txt

./noxim -dimx ${dx} -dimy ${dy} -dimz ${dz} -traffic table traffic_onruntime.txt > output.txt
cp -r results ${MY_PATH}/onruntime_algorithm/
cp ../../../HotSpot-7.0/standard/onruntime_algorithm/mapping/onruntime_report_values.txt ${MY_PATH}/onruntime_algorithm/reference_values.txt
cp output.txt ${MY_PATH}/onruntime_algorithm/report_values.txt
rm -r results
rm output.txt

./noxim -dimx ${dx} -dimy ${dy} -dimz ${dz} -traffic table traffic_pair.txt > output.txt
cp -r results ${MY_PATH}/pair_algorithm/
cp ../../../HotSpot-7.0/standard/pair_algorithm/mapping/pair_algorithm_report_values.txt ${MY_PATH}/pair_algorithm/reference_values.txt
cp output.txt ${MY_PATH}/pair_algorithm/report_values.txt
rm -r results
rm output.txt