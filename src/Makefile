UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), Darwin)
    CXX = /opt/homebrew/opt/llvm/bin/clang++
    CXXFLAGS = -std=c++17 -O3 -ffast-math -fopenmp 
else
    CXX = g++
    CXXFLAGS = -std=c++17 -O3 -march=native -fopenmp -fopenmp-simd -pthread -Wall -Wextra -Wshadow -w -static -ffast-math
endif

all: aku aku_experiment aku_bot

../bin:
	@mkdir -p ../bin

../bin/aku_experiment:
	@mkdir -p ../bin/aku_experiment

D_git_pig_chess_engine_lichess_bot_lichess_bot_engines:
	@mkdir -p D:/git/pig-chess-engine/lichess-bot/lichess-bot/engines

aku: aku.cpp search.cpp  ../lib/fathom/src/tbprobe.c | 
	$(CXX) $(CXXFLAGS) -I include/ -I ../lib/fathom/src -o ../bin/aku/aku $^ -lm

aku_experiment: aku.cpp search_experiment.cpp ../lib/fathom/src/tbprobe.c | ../bin/aku_experiment
	$(CXX) $(CXXFLAGS) -I include/ -I ../lib/fathom/src -o ../bin/aku_experiment/aku_experiment $^ -lm

aku_bot: aku.cpp search_experiment.cpp ../lib/fathom/src/tbprobe.c | D_git_pig_chess_engine_lichess_bot_lichess_bot_engines
	$(CXX) $(CXXFLAGS) -I include/ -I ../lib/fathom/src -o D:/git/pig-chess-engine/lichess-bot/lichess-bot/engines/aku_bot.exe $^ -lm

clean:
	rm -rf ../bin ../lib/fathom/src/*.o

.PHONY: all aku aku_experiment aku_bot clean
