g++ -c -g  shreya_algorithm.cpp -o shreya_algorithm -I /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0/simulation
gcc -c -g  /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0/simulation/thermal_simulator.c -o thermal_simulator -I /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0
g++ -g  shreya_algorithm thermal_simulator -o shreya_algorithm_program -L /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0 -lhotspot -ltinyxml2 -lm
./shreya_algorithm_program ./help/example.xml
