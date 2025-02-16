# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++17 -O3 -fopenmp -march=native

# Directories
BIN_DIR = ../bin

# Source Files
SRC_COMMON = search.cpp evaluation.cpp
SRC_DONBOT = donbot.cpp
SRC_DONBOT_WEB = donbot.cpp
SRC_DEBUG = debug.cpp
SRC_DONBOT_BLITZ = donbot_blitz.cpp

# Output Binaries
BIN_DONBOT = $(BIN_DIR)/donbot
BIN_DONBOT_WEB = $(BIN_DIR)/donbot_web
BIN_DEBUG = $(BIN_DIR)/debug
BIN_DONBOT_BLITZ = $(BIN_DIR)/donbot_blitz

# Build Targets
all: donbot donbot_web debug donbot_blitz

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

donbot: $(SRC_DONBOT) $(SRC_COMMON) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(BIN_DONBOT) $(SRC_DONBOT) $(SRC_COMMON)

donbot_web: $(SRC_DONBOT_WEB) $(SRC_COMMON) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(BIN_DONBOT_WEB) $(SRC_DONBOT_WEB) $(SRC_COMMON)

debug: $(SRC_DEBUG) $(SRC_COMMON) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(BIN_DEBUG) $(SRC_DEBUG) $(SRC_COMMON)

donbot_blitz: $(SRC_DONBOT_BLITZ) $(SRC_COMMON) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(BIN_DONBOT_BLITZ) $(SRC_DONBOT_BLITZ) $(SRC_COMMON)

# Clean
clean:
	rm -rf $(BIN_DIR)

.PHONY: all donbot donbot_web debug donbot_blitz clean
