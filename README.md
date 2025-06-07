This is a side project to refresh my C++ and to test several techniques in the search algorithm to burn some time. There were certainly several silly bugs in the past. I'm also too lazy to write my own movegen at the moment. 

To play with the engine in a GUI, you can use any UCI-compatible GUI such as CuteChess, PyChess, etc., and add the engine to the program.  

The engine currently plays rapid chess at an estimated 3100-3400 CCRL ELO (If I have to guess, it is probably above 3200 and somewhere between 3200-3400. But I have no idea.). The main goal is to see if I can improve the strength through sketching, sampling, or anything that I can come up with. Hopefully, there is a clean algorithm to replace or encapsulate multiple heuristics that requires less finetuning. 

To build, simply call "make aku" from "src" and make sure "bin/aku" folder exists. This is a fun side project so any suggestion is welcome. It should compile on Windows, Linux, and MacOS.

So far, I have nothing really major to share but there are some cute ideas that worked out well. I use Misra-Gries summaries to keep track of frequent pairs of moves (at (ply, ply - 1) and (ply, ply - 2)) that caused beta cut-off or raised alpha to have a more efficient counter-move and follow-up heuristics (~ 45 elo). This heavy-hitter data structure will also allow us to scale to triples, quadruples, etc in the future if they are meaningful. Note that Misra-Gries is used purely to save memory instead of using a hash table of size 64^4. I'm currently trying out several sampling strategies to improve the search. 

Misc:
- The engine has its NNUE inference for the vanilla NNUE (768 -> 1024)x2 -> output architecture. The model was trained using the Bullet library and some Stockfish/Leela's binpacks. If you're not happy about this, this engine's main purpose isn't to play in tournaments although you should not be too upset about what people do with their fun project.
- For the handcrafted evaluation version, visit: [donbot_hce](https://github.com/hoavu-cs/donbot_hce). No longer maintained.


Acknowledgements
- **Bitboard and move generation library**: [chess-library](https://github.com/Disservin/chess-library)
- **Syzygy probe library**: [Fathom](https://github.com/jdart1/Fathom)
- **Utility for including binary files**: [incbin](https://github.com/graphitemaster/incbin)
- **ML library for training NNUE-style networks**: [Bullet](https://github.com/graphitemaster/incbin)
- **SPRT testing**: [FastChess](https://github.com/Disservin/fastchess)
- Thanks to Jim Ablett for several UCI commands implementations.





