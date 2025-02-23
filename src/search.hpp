#pragma once

#include "chess.hpp"

using namespace chess;

// Constants
const int INF = 100000;

// Function Declarations

Move findBestMove(
    Board &board,
    int numThreads,
    int maxDepth,
    int timeLimit,
    bool quiet);
