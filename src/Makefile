UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), Darwin)
    CXX = /opt/homebrew/opt/llvm/bin/clang++
    CXXFLAGS = -std=c++17 -O3 -ffast-math -fopenmp 
else
    CXX = g++
    CXXFLAGS = -std=c++17 -O3 -march=native -fopenmp -pthread -Wall -Wextra -Wshadow -w -static
endif

all: aku debug_nnue aku_experiment debug_experiment

../bin:
	@mkdir -p ../bin

aku: aku.cpp search.cpp utils.cpp ../lib/stockfish_nnue_probe/bitboard.cpp ../lib/stockfish_nnue_probe/evaluate.cpp ../lib/stockfish_nnue_probe/misc.cpp ../lib/stockfish_nnue_probe/position.cpp ../lib/stockfish_nnue_probe/probe.cpp ../lib/stockfish_nnue_probe/nnue/evaluate_nnue.cpp ../lib/stockfish_nnue_probe/nnue/features/half_ka_v2_hm.cpp ../lib/fathom/src/tbprobe.c | ../bin
	$(CXX) $(CXXFLAGS) -I include/ -I ../lib/stockfish_nnue_probe -I ../lib/fathom/src -o ../bin/aku $^ -lm

debug_nnue: debug.cpp search.cpp utils.cpp ../lib/stockfish_nnue_probe/bitboard.cpp ../lib/stockfish_nnue_probe/evaluate.cpp ../lib/stockfish_nnue_probe/misc.cpp ../lib/stockfish_nnue_probe/position.cpp ../lib/stockfish_nnue_probe/probe.cpp ../lib/stockfish_nnue_probe/nnue/evaluate_nnue.cpp ../lib/stockfish_nnue_probe/nnue/features/half_ka_v2_hm.cpp ../lib/fathom/src/tbprobe.c  | ../bin
	$(CXX) $(CXXFLAGS) -I include/ -I ../lib/stockfish_nnue_probe -I ../lib/fathom/src -o ../bin/debug_nnue $^ -lm

aku_experiment: aku.cpp search_experiment.cpp utils.cpp ../lib/stockfish_nnue_probe/bitboard.cpp ../lib/stockfish_nnue_probe/evaluate.cpp ../lib/stockfish_nnue_probe/misc.cpp ../lib/stockfish_nnue_probe/position.cpp ../lib/stockfish_nnue_probe/probe.cpp ../lib/stockfish_nnue_probe/nnue/evaluate_nnue.cpp ../lib/stockfish_nnue_probe/nnue/features/half_ka_v2_hm.cpp ../lib/fathom/src/tbprobe.c  | ../bin
	$(CXX) $(CXXFLAGS) -I include/ -I ../lib/stockfish_nnue_probe -I ../lib/fathom/src -o ../bin/aku_experiment $^ -lm

debug: debug.cpp search_experiment.cpp utils.cpp ../lib/stockfish_nnue_probe/bitboard.cpp ../lib/stockfish_nnue_probe/evaluate.cpp ../lib/stockfish_nnue_probe/misc.cpp ../lib/stockfish_nnue_probe/position.cpp ../lib/stockfish_nnue_probe/probe.cpp ../lib/stockfish_nnue_probe/nnue/evaluate_nnue.cpp ../lib/stockfish_nnue_probe/nnue/features/half_ka_v2_hm.cpp ../lib/fathom/src/tbprobe.c  | ../bin
	$(CXX) $(CXXFLAGS) -I include/ -I ../lib/stockfish_nnue_probe -I ../lib/fathom/src -o ../bin/debug $^ -lm

clean:
	rm -rf ../bin ../lib/Fathom/src/*.o

.PHONY: all aku debug_nnue aku_experiment debug clean
