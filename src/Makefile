# Detect platform
UNAME_S := $(shell uname -s)

# Default Compiler and Flags
ifeq ($(UNAME_S), Darwin)
    CXX = /opt/homebrew/opt/llvm/bin/clang++
    CXXFLAGS = -std=c++17 -O3 -ffast-math -fopenmp
else
    CXX = g++
    CXXFLAGS = -std=c++17 -O3 -march=native -fopenmp -pthread -Wall -Wextra -Wshadow -w
endif

# Directories
BIN_DIR = ../bin
LIB_DIR = ../lib/stockfish_nnue_probe

# Source Files
SRC_COMMON = search.cpp evaluation.cpp
SRC_DONBOT = donbot.cpp
SRC_DEBUG = debug.cpp
SRC_DONBOT_BLITZ = donbot_blitz.cpp
SRC_DONBOT_BLITZ_EVAL = search.cpp evaluation_blitz.cpp
SRC_NNUE = donbot.cpp search_nnue.cpp utils.cpp \
           $(LIB_DIR)/bitboard.cpp \
           $(LIB_DIR)/evaluate.cpp \
           $(LIB_DIR)/misc.cpp \
           $(LIB_DIR)/position.cpp \
           $(LIB_DIR)/probe.cpp \
           $(LIB_DIR)/nnue/evaluate_nnue.cpp \
           $(LIB_DIR)/nnue/features/half_ka_v2_hm.cpp

SRC_DEBUG_NNUE = debug.cpp search_nnue.cpp utils.cpp \
                  $(LIB_DIR)/bitboard.cpp \
                  $(LIB_DIR)/evaluate.cpp \
                  $(LIB_DIR)/misc.cpp \
                  $(LIB_DIR)/position.cpp \
                  $(LIB_DIR)/probe.cpp \
                  $(LIB_DIR)/nnue/evaluate_nnue.cpp \
                  $(LIB_DIR)/nnue/features/half_ka_v2_hm.cpp

# Output Binaries
BIN_DONBOT = $(BIN_DIR)/donbot
BIN_DEBUG = $(BIN_DIR)/debug
BIN_DONBOT_BLITZ = $(BIN_DIR)/donbot_blitz
BIN_DONBOT_NNUE = $(BIN_DIR)/donbot_nnue
BIN_DEBUG_NNUE = $(BIN_DIR)/debug_nnue

# Include Directories
INCLUDE_DIR = -I include/ -I $(LIB_DIR)

# Build Targets
all: donbot debug donbot_blitz donbot_nnue debug_nnue

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

donbot: $(SRC_DONBOT) $(SRC_COMMON) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIR) -o $(BIN_DONBOT) $(SRC_DONBOT) $(SRC_COMMON)

debug: $(SRC_DEBUG) $(SRC_COMMON) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIR) -o $(BIN_DEBUG) $(SRC_DEBUG) $(SRC_COMMON)

donbot_blitz: $(SRC_DONBOT_BLITZ) $(SRC_DONBOT_BLITZ_EVAL) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIR) -o $(BIN_DONBOT_BLITZ) $(SRC_DONBOT_BLITZ) $(SRC_DONBOT_BLITZ_EVAL)

donbot_nnue: $(SRC_NNUE) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIR) -o $(BIN_DONBOT_NNUE) $(SRC_NNUE)

debug_nnue: $(SRC_DEBUG_NNUE) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIR) -o $(BIN_DEBUG_NNUE) $(SRC_DEBUG_NNUE)

# Clean
clean:
	rm -rf $(BIN_DIR)

.PHONY: all donbot debug donbot_blitz donbot_nnue debug_nnue clean
