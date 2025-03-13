all: donbot_nnue debug_nnue donbot_nn_experiment debug_nn_experiment

donbot_nnue:
	g++ -std=c++17 -O3 -march=native -fopenmp -pthread -Wall -Wextra -Wshadow -w \
	    -I include/ -I ../lib/stockfish_nnue_probe \
	    -o ../bin/donbot_nnue \
	    donbot_nnue.cpp search.cpp utils.cpp \
	    ../lib/stockfish_nnue_probe/bitboard.cpp \
	    ../lib/stockfish_nnue_probe/evaluate.cpp \
	    ../lib/stockfish_nnue_probe/misc.cpp \
	    ../lib/stockfish_nnue_probe/position.cpp \
	    ../lib/stockfish_nnue_probe/probe.cpp \
	    ../lib/stockfish_nnue_probe/nnue/evaluate_nnue.cpp \
	    ../lib/stockfish_nnue_probe/nnue/features/half_ka_v2_hm.cpp

debug_nnue:
	g++ -std=c++17 -O3 -march=native -fopenmp -pthread -Wall -Wextra -Wshadow -w \
	    -I include/ -I ../lib/stockfish_nnue_probe \
	    -o ../bin/debug_nnue \
	    debug.cpp search.cpp utils.cpp \
	    ../lib/stockfish_nnue_probe/bitboard.cpp \
	    ../lib/stockfish_nnue_probe/evaluate.cpp \
	    ../lib/stockfish_nnue_probe/misc.cpp \
	    ../lib/stockfish_nnue_probe/position.cpp \
	    ../lib/stockfish_nnue_probe/probe.cpp \
	    ../lib/stockfish_nnue_probe/nnue/evaluate_nnue.cpp \
	    ../lib/stockfish_nnue_probe/nnue/features/half_ka_v2_hm.cpp

donbot_nn_experiment:
	g++ -std=c++17 -O3 -march=native -fopenmp -pthread -Wall -Wextra -Wshadow -w \
	    -I include/ -I ../lib/stockfish_nnue_probe \
	    -o ../bin/donbot_nn_experiment \
	    donbot_nnue.cpp search_experiment.cpp utils.cpp \
	    ../lib/stockfish_nnue_probe/bitboard.cpp \
	    ../lib/stockfish_nnue_probe/evaluate.cpp \
	    ../lib/stockfish_nnue_probe/misc.cpp \
	    ../lib/stockfish_nnue_probe/position.cpp \
	    ../lib/stockfish_nnue_probe/probe.cpp \
	    ../lib/stockfish_nnue_probe/nnue/evaluate_nnue.cpp \
	    ../lib/stockfish_nnue_probe/nnue/features/half_ka_v2_hm.cpp

debug_nn_experiment:
	g++ -std=c++17 -O3 -march=native -fopenmp -pthread -Wall -Wextra -Wshadow -w \
	    -I include/ -I ../lib/stockfish_nnue_probe \
	    -o ../bin/debug_nn_experiment \
	    debug.cpp search_experiment.cpp utils.cpp \
	    ../lib/stockfish_nnue_probe/bitboard.cpp \
	    ../lib/stockfish_nnue_probe/evaluate.cpp \
	    ../lib/stockfish_nnue_probe/misc.cpp \
	    ../lib/stockfish_nnue_probe/position.cpp \
	    ../lib/stockfish_nnue_probe/probe.cpp \
	    ../lib/stockfish_nnue_probe/nnue/evaluate_nnue.cpp \
	    ../lib/stockfish_nnue_probe/nnue/features/half_ka_v2_hm.cpp

clean:
	rm -rf ../bin

.PHONY: all donbot_nnue debug_nnue donbot_nn_experiment debug_nn_experiment clean
