This engine was renamed from **Donbot** to **Aku**

# How to Play with the Engine in a GUI

To play on a GUI, you can use any UCI-compatible GUI such as **Cute Chess**, **PyChess**, **Nibbler**, etc., and add the engine to the GUI program.  
The binaries for **Windows** and **MacOS** can be downloaded from the [releases](https://github.com/hoavu-cs/donbot-chess-engine/releases/).

A few interesting features
- Support Windows, MacOS, and Linux.
- Self-contained 3-4 endgame tablebases.
- NNUE evaluation.

## Strength and Performance

The engine currently plays **rapid chess** at an estimated **~3100-3400 ELO** (subject to further testing).  
Some benchmark match results:

- **vs Glaurung 2.2** with a score **5W-1D** in a 6-game match with time control 15/40.
- **vs Houdini 1.5a** with a score **4W-4D** in a 8-game match time control 15/40.

## Evaluation Method

This engine uses **NNUE (Efficiently Updatable Neural Network) evaluation**.  
For the handcrafted evaluation version, visit: [donbot_hce](https://github.com/hoavu-cs/donbot_hce).

## Online Version

Play online at: [donbotchess.org](https://donbotchess.org/)

## Acknowledgements

- **NNUE probe library**: [stockfish_nnue_probe](https://github.com/VedantJoshi1409/stockfish_nnue_probe)
- **Bitboard and move generation library**: [chess-library](https://github.com/Disservin/chess-library)
- **Syzygy probe library**: [Fathom](https://github.com/jdart1/Fathom)
- **Utility for including binary files**: [incbin](https://github.com/graphitemaster/incbin)
