This is a side project to test several ideas in the search algorithm. There were certainly several silly bugs in the past. I'm also too lazy to write my own movegen at the moment. 

To play with the engine in a GUI, you can use any UCI-compatible GUI such as CuteChess, PyChess, etc., and add the engine to the program.  

The engine currently plays rapid chess at an estimated 3100-3300 ELO. The main goal is to improve the strength through exploring new ideas in the search algorithm (more specifically, sampling and sketching ideas from my research). Hopefully, there is a clean algorithm to replace or encapsulate multiple heuristics that requires less finetuning. 

To build, simply call "make aku" from "src" and make sure "bin/aku" folder exists. This is a fun side project so any suggestion is welcome. It should compile on Windows, Linux, and MacOS.

So far, I have nothing really major to report but there are some cute ideas that worked out well. I use Misra-Gries to keep track of frequent pairs of moves (at (ply, ply - 1) and (ply, ply - 2)) that caused beta cut-off or raised alpha to have a more efficient counter-move and follow-up heuristics (~ 45 elo). This heavy-hitter data structure will also allow us to scale to triples, quadruples, etc in the future. Note that Misra-Gries is used purely to save memory instead of using a hash table of size 64^4. 

I'm trying out several sampling strategies. 

Misc:
- The engine has its NNUE inference for the vanilla NNUE (768 -> 1024)x2 -> output architecture. The model was trained using the Bullet library and some Stockfish/Leela's binpacks. If you're not happy about this, this engine's main purpose isn't to play in tournaments (although I'm not against it either).
- For the handcrafted evaluation version, visit: [donbot_hce](https://github.com/hoavu-cs/donbot_hce). No longer maintained.


Acknowledgements
- **Bitboard and move generation library**: [chess-library](https://github.com/Disservin/chess-library)
- **Syzygy probe library**: [Fathom](https://github.com/jdart1/Fathom)
- **Utility for including binary files**: [incbin](https://github.com/graphitemaster/incbin)
- **ML library for training NNUE-style networks**: [Bullet](https://github.com/graphitemaster/incbin)
- Thanks to Jim Ablett for several implementation improvement of the UCI commands.





