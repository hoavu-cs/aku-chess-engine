This engine was renamed from **Donbot** to **Aku**

# How to Play with the Engine in a GUI

To play on a GUI, you can use any UCI-compatible GUI such as **CuteChess**, **PyChess**, etc., and add the engine to the GUI program.  
The binaries for **Windows** and **MacOS** can be downloaded from the [releases](https://github.com/hoavu-cs/donbot-chess-engine/releases/) or just download the repository and call "make aku" from inside the src folder.

A few interesting features
- Support Windows, MacOS, and Linux.
- Self-contained 3-4 endgame tablebases.
- NNUE evaluation.
- Supports chess960.
- Lightweight < 10Mb.

## Strength and Performance

The engine currently plays rapid chess at an estimated **~3100-3400 ELO** (subject to further testing). The main goal is to improve the strength through exploring new ideas in the search algorithm. In my opinion, there should be a clean search algorithm to replace or encapsulate multiple heuristics that take a lot of manual effort in finetuning (i.e., I want to bypass this as much as possible).

This is a fun side project so any suggestion is welcome. Currently, the engine is pretty strong and based on some simple techniques and NNUE evaluation. 

## Evaluation Method

- This engine uses NNUE (Efficiently Updatable Neural Network) evaluation.  

- The engine has its own NNUE inference implementation for the vanilla NNUE (768 -> 512)x2 -> output architecture. The model was trained using the Bullet library and some Stockfish/Leela's binpacks. Currently trying larger architecture.

- For the handcrafted evaluation version, visit: [donbot_hce](https://github.com/hoavu-cs/donbot_hce).

## Online Version

Play online at: [Lichess](https://lichess.org/@/AkuBot)

## Acknowledgements

- **Bitboard and move generation library**: [chess-library](https://github.com/Disservin/chess-library)
- **Syzygy probe library**: [Fathom](https://github.com/jdart1/Fathom)
- **Utility for including binary files**: [incbin](https://github.com/graphitemaster/incbin)
- **ML library for training NNUE-style networks**: [Bullet](https://github.com/graphitemaster/incbin) 


