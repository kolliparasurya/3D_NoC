g++ -c link.cpp -o link -I ../../
gcc -c -g  ../../thermal_simulator.c -o thermal_simulator -I ../../../
g++ -g  link thermal_simulator -o my_program -L ../../../ -lhotspot -ltinyxml2 -lm
./my_program 0
cp avg.steady avg.init
# ./my_program 1