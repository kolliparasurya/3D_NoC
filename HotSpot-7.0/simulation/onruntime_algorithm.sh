g++ -c -g  onruntime_algorithm.cpp -o onruntime_algorithm -I /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0/simulation
gcc -c -g  /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0/simulation/thermal_simulator.c -o thermal_simulator -I /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0
g++ -g  onruntime_algorithm thermal_simulator -o onruntime_algorithm_program -L /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0 -lhotspot -ltinyxml2 -lm
./onruntime_algorithm_program ./help/example.xml
