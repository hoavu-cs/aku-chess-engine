UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), Darwin)
    CXX = /opt/homebrew/opt/llvm/bin/clang++
    CXXFLAGS = -std=c++17 -O3 -ffast-math -fopenmp 
else
    CXX = g++
    CXXFLAGS = -std=c++17 -O3 -march=native -fopenmp -fopenmp-simd -pthread -Wall -Wextra -Wshadow -w -static
endif

all: aku debug_nnue aku_experiment

../bin:
	@mkdir -p ../bin

aku_experiment: aku.cpp search_experiment.cpp  ../lib/fathom/src/tbprobe.c  | ../bin
	$(CXX) $(CXXFLAGS) -I include/ -I ../lib/fathom/src -o ../bin/aku_experiment $^ -lm

aku: aku.cpp search.cpp  ../lib/fathom/src/tbprobe.c | ../bin
	$(CXX) $(CXXFLAGS) -I include/ -I ../lib/fathom/src -o ../bin/aku $^ -lm

debug: debug.cpp search_experiment.cpp   ../lib/fathom/src/tbprobe.c  | ../bin
	$(CXX) $(CXXFLAGS) -I include/ -I ../lib/fathom/src -o ../bin/debug $^ -lm

clean:
	rm -rf ../bin ../lib/Fathom/src/*.o

.PHONY: all aku debug_nnue aku_experiment debug clean
