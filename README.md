This is a fun side project to test several searching algorithms; it's not an attempt at being super competitive (though I will keep making it stronger when I have time). There were certainly several silly bugs in the implementation and logic. 

How to Play with the Engine in a GUI: you can use any UCI-compatible GUI such as CuteChess, PyChess, etc., and add the engine to the GUI program.  

The engine currently plays rapid chess at an estimated 3100-3300 ELO (subject to further testing and vary greatly because I often make random changes to test new ideas). The main goal is to improve the strength through exploring new ideas in the search algorithm. In my opinion, there should be a clean search algorithm to replace or encapsulate multiple heuristics that take a lot of manual effort in finetuning (i.e., I want to bypass this as much as possible). 

This is a fun side project so any suggestion is welcome. 

- This engine uses NNUE (Efficiently Updatable Neural Network) evaluation.  
- The engine has its NNUE inference for the vanilla NNUE (768 -> 512)x2 -> output architecture. The model was trained using the Bullet library and some Stockfish/Leela's binpacks. 
- For the handcrafted evaluation version, visit: [donbot_hce](https://github.com/hoavu-cs/donbot_hce).


Acknowledgements

- **Bitboard and move generation library**: [chess-library](https://github.com/Disservin/chess-library)
- **Syzygy probe library**: [Fathom](https://github.com/jdart1/Fathom)
- **Utility for including binary files**: [incbin](https://github.com/graphitemaster/incbin)
- **ML library for training NNUE-style networks**: [Bullet](https://github.com/graphitemaster/incbin) 


