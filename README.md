Go to src and compile:

g++ -std=c++17 -O3 -march=native -o pigengine pig_engine.cpp search.cpp evaluation_utils.cpp  

To play on a GUI, you can use any UCI engine such as Lucas chess, pychess, nibbler,... and add the engine to the GUI program.

The engine can currently play rapid chess around 2200-2400 ELO.
