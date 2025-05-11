UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), Darwin)
    CXX = /opt/homebrew/opt/llvm/bin/clang++
    CXXFLAGS = -std=c++17 -O3 -ffast-math -fopenmp 
else
    CXX = g++
    CXXFLAGS = -std=c++17 -O3 -march=native -fopenmp -fopenmp-simd -pthread -Wall -Wextra -Wshadow -w -static
endif

all: aku aku_experiment

../bin:
	@mkdir -p ../bin

../bin/aku_experiment:
	@mkdir -p ../bin/aku_experiment

aku: aku.cpp search.cpp ../lib/fathom/src/tbprobe.c | ../bin
	$(CXX) $(CXXFLAGS) -I include/ -I ../lib/fathom/src -o ../bin/aku/aku $^ -lm

aku_experiment: aku.cpp search_experiment.cpp ../lib/fathom/src/tbprobe.c | ../bin/aku_experiment
	$(CXX) $(CXXFLAGS) -I include/ -I ../lib/fathom/src -o ../bin/aku_experiment/aku_experiment $^ -lm

clean:
	rm -rf ../bin ../lib/fathom/src/*.o

.PHONY: all aku aku_experiment clean
