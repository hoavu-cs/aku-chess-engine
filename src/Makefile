# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++17 -O3 -fopenmp -march=native

# Source Files
SRC_COMMON = search.cpp evaluation.cpp
SRC_DONBOT = donbot.cpp
SRC_DONBOT_WEB = donbot.cpp
SRC_DEBUG = debug.cpp
SRC_DONBOT_BLITZ = donbot_blitz.cpp

# Output Binaries
BIN_DIR = ../bin
BIN_DONBOT = $(BIN_DIR)/donbot
BIN_DONBOT_WEB = donbot_web
BIN_DEBUG = debug
BIN_DONBOT_BLITZ = donbot_blitz

# Build Targets
all: donbot donbot_web debug donbot_blitz

donbot: $(SRC_DONBOT) $(SRC_COMMON)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(BIN_DONBOT) $(SRC_DONBOT) $(SRC_COMMON)

donbot_web: $(SRC_DONBOT_WEB) $(SRC_COMMON)
	$(CXX) $(CXXFLAGS) -o $(BIN_DONBOT_WEB) $(SRC_DONBOT_WEB) $(SRC_COMMON)

debug: $(SRC_DEBUG) $(SRC_COMMON)
	$(CXX) $(CXXFLAGS) -o $(BIN_DEBUG) $(SRC_DEBUG) $(SRC_COMMON)

donbot_blitz: $(SRC_DONBOT_BLITZ) $(SRC_COMMON)
	$(CXX) $(CXXFLAGS) -o $(BIN_DONBOT_BLITZ) $(SRC_DONBOT_BLITZ) $(SRC_COMMON)

# Clean
clean:
	rm -f $(BIN_DONBOT) $(BIN_DONBOT_WEB) $(BIN_DEBUG) $(BIN_DONBOT_BLITZ)

.PHONY: all donbot donbot_web debug donbot_blitz clean
