This is a fun side project to test several searching algorithms; it's not an attempt at being super competitive (though that doesn't stop me from it stronger). There were certainly several silly bugs in the implementation and logic in the past. I'm also too lazy to write my own movegen at the moment.

To Play with the Engine in a GUI, you can use any UCI-compatible GUI such as CuteChess, PyChess, etc., and add the engine to the GUI program.  

The engine currently plays rapid chess at an estimated 3100-3300 ELO (subject to further testing and vary greatly because I often make random changes to test new ideas & some are really dumb). The main goal is to improve the strength through exploring new ideas in the search algorithm. In my opinion, there should be a clean search algorithm to replace or encapsulate multiple heuristics that take a lot of manual effort in finetuning (i.e., I want to bypass this as much as possible). 

So far, nothing really major but some cute ideas that work:
- I use Misra-Gries to keep track of frequent pairs of moves (at (ply, ply - 1) and (ply, ply - 2)) that caused beta cut-offf to have a more efficient and counter-move and follow-up heuristics (~ 45 elo). This heavy-hitter data structure will also allow us to scale to triples, quadruples, etc in the future. Note that Misra-Gries is used purely to save memory instead of using a hash table of size 64^4.


To build, simply call "make aku" from "src" and make sure "bin/aku" folder exists. This is a fun side project so any suggestion is welcome. 

- This engine uses NNUE (Efficiently Updatable Neural Network) evaluation.  
- The engine has its NNUE inference for the vanilla NNUE (768 -> 1024)x2 -> output architecture. The model was trained using the Bullet library and some Stockfish/Leela's binpacks. 
- For the handcrafted evaluation version, visit: [donbot_hce](https://github.com/hoavu-cs/donbot_hce).


Acknowledgements

- **Bitboard and move generation library**: [chess-library](https://github.com/Disservin/chess-library)
- **Syzygy probe library**: [Fathom](https://github.com/jdart1/Fathom)
- **Utility for including binary files**: [incbin](https://github.com/graphitemaster/incbin)
- **ML library for training NNUE-style networks**: [Bullet](https://github.com/graphitemaster/incbin)
- Thanks to Jim Ablett for several implementation improvement suggestion (lazysmp and uci commands).





