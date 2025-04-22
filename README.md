This engine was renamed from **Donbot** to **Aku**

# How to Play with the Engine in a GUI

To play on a GUI, you can use any UCI-compatible GUI such as **Cute Chess**, **PyChess**, **Nibbler**, etc., and add the engine to the GUI program.  
The binaries for **Windows** and **MacOS** can be downloaded from the [releases](https://github.com/hoavu-cs/donbot-chess-engine/releases/) or just download the repository and call "make aku" from inside the src folder.

A few interesting features
- Support Windows, MacOS, and Linux.
- Self-contained 3-4 endgame tablebases.
- NNUE evaluation. In the process of training my own NNUE.
- Supports chess960.

## Strength and Performance

The engine currently plays rapid chess at an estimated **~3100-3400 ELO** (subject to further testing). The main goal is to improve the strength through **exploring new ideas** mainly in the **search algorithm**. In my opinion, there should be a clean search algorithm to replace or encapsulate multiple heuristics that take a lot of manual effort in finetuning (i.e., I want to bypass this as much as possible).

Though my current focus for this engine is the search algorithm, I'd like to train my own NNUE at some point. 

Current ideas I would like to explore:
- Instead of finetuning parameters heavily, can we use some sort of ensemble methods?
- ML-based move ordering.
- Probabilistic pruning.

I'm new to chess development so any suggestion is welcome.

Currently, the engine is pretty strong and based on some simple concepts: 

- Alpha-beta search, iterative deepening, and transposition tables
- Futility pruning
- Reverse futility pruning
- Null move pruning
- History score 
- Killer move (1 slot per ply)
- Principle variation search
- Late move reduction
- Extensions
- LazySMP

Progress is currently tracked using Sequential Probability Ratio Test [SPRT LOG](https://github.com/hoavu-cs/aku-chess-engine/tree/main/sprt).

## Evaluation Method

This engine uses **NNUE (Efficiently Updatable Neural Network) evaluation**.  
For the handcrafted evaluation version, visit: [donbot_hce](https://github.com/hoavu-cs/donbot_hce).

## Online Version

Play online at: [donbotchess.org](https://donbotchess.org/) and at [Lichess](https://lichess.org/@/AkuBot)

## Acknowledgements

- **NNUE probe library**: [stockfish_nnue_probe](https://github.com/VedantJoshi1409/stockfish_nnue_probe)
- **Bitboard and move generation library**: [chess-library](https://github.com/Disservin/chess-library)
- **Syzygy probe library**: [Fathom](https://github.com/jdart1/Fathom)
- **Utility for including binary files**: [incbin](https://github.com/graphitemaster/incbin)
