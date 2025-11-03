g++ -c buf.cpp -o buf.o -I /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0/simulation
gcc -c /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0/simulation/thermal_simulator.c -o thermal_simulator.o -I /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0
g++ buf.o thermal_simulator.o -o my_program.o -L /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0 -lhotspot -ltinyxml2 -lm
./my_program.o ./output/example.xml