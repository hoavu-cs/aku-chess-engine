UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), Darwin)
    CXX = /opt/homebrew/opt/llvm/bin/clang++
    CXXFLAGS = -std=c++17 -O3 -ffast-math -fopenmp
else
    CXX = g++
    CXXFLAGS = -std=c++17 -O3 -march=native -fopenmp -pthread -Wall -Wextra -Wshadow -w
endif

all: donbot_nnue debug_nnue donbot_nn_experiment debug_nn_experiment

../bin:
	@mkdir -p ../bin

donbot_nnue: donbot_nnue.cpp search.cpp utils.cpp ../lib/stockfish_nnue_probe/bitboard.cpp ../lib/stockfish_nnue_probe/evaluate.cpp ../lib/stockfish_nnue_probe/misc.cpp ../lib/stockfish_nnue_probe/position.cpp ../lib/stockfish_nnue_probe/probe.cpp ../lib/stockfish_nnue_probe/nnue/evaluate_nnue.cpp ../lib/stockfish_nnue_probe/nnue/features/half_ka_v2_hm.cpp | ../bin
	$(CXX) $(CXXFLAGS) -I ../lib/Fathom/src -I include/ -I ../lib/stockfish_nnue_probe -o ../bin/donbot_nnue $^

debug_nnue: debug.cpp search.cpp utils.cpp ../lib/stockfish_nnue_probe/bitboard.cpp ../lib/stockfish_nnue_probe/evaluate.cpp ../lib/stockfish_nnue_probe/misc.cpp ../lib/stockfish_nnue_probe/position.cpp ../lib/stockfish_nnue_probe/probe.cpp ../lib/stockfish_nnue_probe/nnue/evaluate_nnue.cpp ../lib/stockfish_nnue_probe/nnue/features/half_ka_v2_hm.cpp | ../bin
	$(CXX) $(CXXFLAGS) -I ../lib/Fathom/src -I include/ -I ../lib/stockfish_nnue_probe -o ../bin/debug_nnue $^

donbot_nn_experiment: donbot_nnue.cpp search_experiment.cpp utils.cpp ../lib/stockfish_nnue_probe/bitboard.cpp ../lib/stockfish_nnue_probe/evaluate.cpp ../lib/stockfish_nnue_probe/misc.cpp ../lib/stockfish_nnue_probe/position.cpp ../lib/stockfish_nnue_probe/probe.cpp ../lib/stockfish_nnue_probe/nnue/evaluate_nnue.cpp ../lib/stockfish_nnue_probe/nnue/features/half_ka_v2_hm.cpp | ../bin
	$(CXX) $(CXXFLAGS) -I ../lib/Fathom/src -I include/ -I ../lib/stockfish_nnue_probe -o ../bin/donbot_nn_experiment $^

debug_nn_experiment: debug.cpp search_experiment.cpp utils.cpp ../lib/stockfish_nnue_probe/bitboard.cpp ../lib/stockfish_nnue_probe/evaluate.cpp ../lib/stockfish_nnue_probe/misc.cpp ../lib/stockfish_nnue_probe/position.cpp ../lib/stockfish_nnue_probe/probe.cpp ../lib/stockfish_nnue_probe/nnue/evaluate_nnue.cpp ../lib/stockfish_nnue_probe/nnue/features/half_ka_v2_hm.cpp | ../bin
	$(CXX) $(CXXFLAGS) -I ../lib/Fathom/src -I include/ -I ../lib/stockfish_nnue_probe -o ../bin/debug_nn_experiment $^

clean:
	rm -rf ../bin

.PHONY: all donbot_nnue debug_nnue donbot_nn_experiment debug_nn_experiment clean