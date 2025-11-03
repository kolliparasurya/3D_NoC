g++ -c link.cpp -o link
gcc -c thermal_simulator.c -o thermal_simulator -I /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0
g++ link thermal_simulator -o my_program -L /home/kolliparasurya/college/sem6/project1/samplecode/Noc_RL_MAP/HotSpot-7.0 -lhotspot -lm
./my_program 0
cp avg.steady avg.init
./my_program 1