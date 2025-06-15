This is a side project to refresh my C++ and to test several techniques in the search algorithm to burn some time. I'm also too lazy to write my own movegen at the moment. To play with the engine in a GUI, you can use any UCI-compatible GUI such as CuteChess, PyChess, Nibbler etc., and add the engine to the program. To build the source, simply call "make aku" from "src" and make sure "bin/aku" folder exists. It should compile on Windows, Linux, and MacOS for C++ 17.

**Strength**. The engine currently plays rapid chess at an estimated 3100-3400 CCRL ELO (If I have to guess, it is probably above 3200 and somewhere between 3200-3400 based on the games against rated engines on Lichess. But I have no idea because one has to factor in the hardware difference and other stuff like pondering).

**Some ideas**. Most stuff I tried did not work out or have little impact. However, one interesting trick that worked well for me is the use of Misra-Gries summaries to keep track of frequent pairs of moves (at (ply, ply - 1) and (ply, ply - 2)) that cause beta cut-off or raise alpha to have a memory-efficient counter-move and follow-up heuristics (~ 45 elo). One can try to make this work for triples or quadruples of moves.

**Misc**. The engine has its NNUE inference for the vanilla (768 -> 1024)x2 architecture. The model was trained using the Bullet library and some Stockfish/Leela's binpacks. If you're not happy about this, this engine's main purpose isn't to play in tournaments (although it's not worth being too upset about a toy project).

I currently have less time for this engine but any suggestion is still welcome. I'm mainly interested in any sort of algorithmic ideas (especially randomized methods) in the search and still doing some experiments and minor tweaks here and there.

**Acknowledgements**
- Bitboard and move generation library: [chess-library](https://github.com/Disservin/chess-library)
- Syzygy probe library: [Fathom](https://github.com/jdart1/Fathom)
- Utility for including binary files: [incbin](https://github.com/graphitemaster/incbin)
- ML library for training NNUE-style networks: [Bullet](https://github.com/graphitemaster/incbin)
- SPRT: [FastChess](https://github.com/Disservin/fastchess)
- Thanks to Jim Ablett for several UCI command implementations.





