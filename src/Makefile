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
SRC_NNUE = donbot_nnue.cpp search.cpp utils.cpp \
           $(LIB_DIR)/bitboard.cpp \
           $(LIB_DIR)/evaluate.cpp \
           $(LIB_DIR)/misc.cpp \
           $(LIB_DIR)/position.cpp \
           $(LIB_DIR)/probe.cpp \
           $(LIB_DIR)/nnue/evaluate_nnue.cpp \
           $(LIB_DIR)/nnue/features/half_ka_v2_hm.cpp

SRC_DEBUG_NNUE = debug.cpp search.cpp utils.cpp \
                  $(LIB_DIR)/bitboard.cpp \
                  $(LIB_DIR)/evaluate.cpp \
                  $(LIB_DIR)/misc.cpp \
                  $(LIB_DIR)/position.cpp \
                  $(LIB_DIR)/probe.cpp \
                  $(LIB_DIR)/nnue/evaluate_nnue.cpp \
                  $(LIB_DIR)/nnue/features/half_ka_v2_hm.cpp

# Source Files for experiment (using search_experiment.cpp)
SRC_NNUE_EXP = donbot_nnue.cpp search_experiment.cpp utils.cpp \
           $(LIB_DIR)/bitboard.cpp \
           $(LIB_DIR)/evaluate.cpp \
           $(LIB_DIR)/misc.cpp \
           $(LIB_DIR)/position.cpp \
           $(LIB_DIR)/probe.cpp \
           $(LIB_DIR)/nnue/evaluate_nnue.cpp \
           $(LIB_DIR)/nnue/features/half_ka_v2_hm.cpp

SRC_DEBUG_NNUE_EXP = debug.cpp search_experiment.cpp utils.cpp \
                  $(LIB_DIR)/bitboard.cpp \
                  $(LIB_DIR)/evaluate.cpp \
                  $(LIB_DIR)/misc.cpp \
                  $(LIB_DIR)/position.cpp \
                  $(LIB_DIR)/probe.cpp \
                  $(LIB_DIR)/nnue/evaluate_nnue.cpp \
                  $(LIB_DIR)/nnue/features/half_ka_v2_hm.cpp

# Output Binaries
BIN_DONBOT_NNUE = $(BIN_DIR)/donbot_nnue
BIN_DEBUG_NNUE = $(BIN_DIR)/debug_nnue
BIN_DONBOT_NN_EXPERIMENT = $(BIN_DIR)/donbot_nn_experiment
BIN_DEBUG_NN_EXPERIMENT = $(BIN_DIR)/debug_nn_experiment

# Include Directories
INCLUDE_DIR = -I include/ -I $(LIB_DIR)

# Build Targets
all: donbot_nnue debug_nnue donbot_nn_experiment debug_nn_experiment

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

donbot_nnue: $(SRC_NNUE) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIR) -o $(BIN_DONBOT_NNUE) $(SRC_NNUE)

debug_nnue: $(SRC_DEBUG_NNUE) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIR) -o $(BIN_DEBUG_NNUE) $(SRC_DEBUG_NNUE)

donbot_nn_experiment: $(SRC_NNUE_EXP) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIR) -o $(BIN_DONBOT_NN_EXPERIMENT) $(SRC_NNUE_EXP)

debug_nn_experiment: $(SRC_DEBUG_NNUE_EXP) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIR) -o $(BIN_DEBUG_NN_EXPERIMENT) $(SRC_DEBUG_NNUE_EXP)

# Clean
clean:
	rm -rf $(BIN_DIR)

.PHONY: all donbot_nnue debug_nnue donbot_nn_experiment debug_nn_experiment clean