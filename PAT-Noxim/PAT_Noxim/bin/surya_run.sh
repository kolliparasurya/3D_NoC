#!/bin/bash

MY_PATH=$1
dx=$2
dy=$3
dz=$4

# Added checks to ensure all dimensions are provided
if [ -z "$MY_PATH" ] || [ -z "$dx" ] || [ -z "$dy" ] || [ -z "$dz" ]; then
    echo "Error: Missing arguments."
    echo "Usage: ./run.sh <path> <dimx> <dimy> <dimz>"
    exit 1
fi

export LD_LIBRARY_PATH=../../../systemc-3.0.1/lib-linux64:$LD_LIBRARY_PATH

# 1. Copy traffic files
cp ../../../HotSpot-7.0/standard/hybrid_dpso_algorithm/mapping/test_traffic.txt traffic_hdpso.txt
cp ../../../HotSpot-7.0/standard/onruntime_algorithm/mapping/test_traffic.txt traffic_onruntime.txt
cp ../../../HotSpot-7.0/standard/pair_algorithm/mapping/test_traffic.txt traffic_pair.txt

# ==========================================
# 2. HYBRID DPSO ALGORITHM
# ==========================================
H_PATH="${MY_PATH}/hybrid_dpso_algorithm"
# Clean up previous runs and make fresh directory structure
rm -rf "${H_PATH}"
mkdir -p "${H_PATH}"

./noxim -dimx ${dx} -dimy ${dy} -dimz ${dz} -traffic table traffic_hdpso.txt > output.txt

cp -r results "${H_PATH}/"
cp ../../../HotSpot-7.0/standard/hybrid_dpso_algorithm/mapping/hybrid_dpso_report_values.txt "${H_PATH}/reference_values.txt"
cp output.txt "${H_PATH}/report_values.txt"

rm -rf results output.txt

# ==========================================
# 3. ON-RUNTIME ALGORITHM
# ==========================================
O_PATH="${MY_PATH}/onruntime_algorithm"
# Clean up previous runs and make fresh directory structure
rm -rf "${O_PATH}"
mkdir -p "${O_PATH}"

./noxim -dimx ${dx} -dimy ${dy} -dimz ${dz} -traffic table traffic_onruntime.txt > output.txt

cp -r results "${O_PATH}/"
cp ../../../HotSpot-7.0/standard/onruntime_algorithm/mapping/onruntime_report_values.txt "${O_PATH}/reference_values.txt"
cp output.txt "${O_PATH}/report_values.txt"

rm -rf results output.txt

# ==========================================
# 4. PAIR ALGORITHM
# ==========================================
P_PATH="${MY_PATH}/pair_algorithm"
# Clean up previous runs and make fresh directory structure
rm -rf "${P_PATH}"
mkdir -p "${P_PATH}"

./noxim -dimx ${dx} -dimy ${dy} -dimz ${dz} -traffic table traffic_pair.txt > output.txt

cp -r results "${P_PATH}/"
cp ../../../HotSpot-7.0/standard/pair_algorithm/mapping/pair_algorithm_report_values.txt "${P_PATH}/reference_values.txt"
cp output.txt "${P_PATH}/report_values.txt"

rm -rf results output.txt

# Clean up local traffic files after simulation
# rm -f traffic_hdpso.txt traffic_onruntime.txt traffic_pair.txt

echo "Simulations complete! Fresh results successfully saved to ${MY_PATH}"


# #!/bin/bash

# MY_PATH=$1
# dx=$2
# dy=$3
# dz=$4
# if [ -z "$MY_PATH" ]; then
#     echo "Error: No path provided."
#     echo "Usage: ./run.sh <path>"
#     exit 1
# fi
# export LD_LIBRARY_PATH=../../../systemc-3.0.1/lib-linux64:$LD_LIBRARY_PATH

# cp ../../../HotSpot-7.0/standard/hybrid_dpso_algorithm/mapping/test_traffic.txt traffic_hdpso.txt
# cp ../../../HotSpot-7.0/standard/onruntime_algorithm/mapping/test_traffic.txt traffic_onruntime.txt
# cp ../../../HotSpot-7.0/standard/pair_algorithm/mapping/test_traffic.txt traffic_pair.txt

# ./noxim -dimx ${dx} -dimy ${dy} -dimz ${dz} -traffic table traffic_hdpso.txt > output.txt
# cp -r results ${MY_PATH}/hybrid_dpso_algorithm/
# cp ../../../HotSpot-7.0/standard/hybrid_dpso_algorithm/mapping/hybrid_dpso_report_values.txt ${MY_PATH}/hybrid_dpso_algorithm/reference_values.txt
# cp output.txt ${MY_PATH}/hybrid_dpso_algorithm/report_values.txt
# rm -r results
# rm output.txt

# ./noxim -dimx ${dx} -dimy ${dy} -dimz ${dz} -traffic table traffic_onruntime.txt > output.txt
# cp -r results ${MY_PATH}/onruntime_algorithm/
# cp ../../../HotSpot-7.0/standard/onruntime_algorithm/mapping/onruntime_report_values.txt ${MY_PATH}/onruntime_algorithm/reference_values.txt
# cp output.txt ${MY_PATH}/onruntime_algorithm/report_values.txt
# rm -r results
# rm output.txt

# ./noxim -dimx ${dx} -dimy ${dy} -dimz ${dz} -traffic table traffic_pair.txt > output.txt
# cp -r results ${MY_PATH}/pair_algorithm/
# cp ../../../HotSpot-7.0/standard/pair_algorithm/mapping/pair_algorithm_report_values.txt ${MY_PATH}/pair_algorithm/reference_values.txt
# cp output.txt ${MY_PATH}/pair_algorithm/report_values.txt
# rm -r results
# rm output.txt