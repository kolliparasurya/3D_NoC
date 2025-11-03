#!/bin/bash
# g++ -c buf.cpp -o buf
# gcc -c thermal_simulator.c -o thermal_simulator -I /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0
# g++ buf thermal_simulator -o my_program -L /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0 -lhotspot -lm
# ./my_program ./help/example.xml

g++ -c -g  pair_algorithm.cpp -o pair_algorithm -I /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0/simulation
gcc -c -g  /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0/simulation/thermal_simulator.c -o thermal_simulator -I /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0
g++ -g  pair_algorithm thermal_simulator -o pair_algorithm_program -L /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0 -lhotspot -ltinyxml2 -lm
./pair_algorithm_program ./help/graph.xml