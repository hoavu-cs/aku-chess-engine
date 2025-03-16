To play on a GUI, you can use any UCI GUI such as cute chess, pychess, nibbler,... and add the engine to the GUI program.
The binaries for Windows and MacOS can be download from the releases.

The engine can currently play rapid chess around ~3100-3400 ELO (subject to further testing). 
- It beats Glaurung 2.2 with a score 5W-1D in a 6-game match.
- It beats Houdini 1.5a with a score 4W-2D in a 6-game match.

This uses NNUE evaluation. The handcrafted evaluation version is now at https://github.com/hoavu-cs/donbot_hce.

Online version at https://donbotchess.org/

Acknowledgement:

NNUE probe library: https://github.com/VedantJoshi1409/stockfish_nnue_probe
Bitboard and movegen library: https://github.com/Disservin/chess-library
Syzygy probe library https://github.com/jdart1/Fathom

