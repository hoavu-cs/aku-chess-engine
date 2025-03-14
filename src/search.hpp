#pragma once

#include "chess.hpp"

using namespace chess;

// Constants
const int INF = 100000;

// Function Declarations
void initializeNNUE();

int negamax(Board& board, 
    int depth, 
    int alpha, 
    int beta, 
    std::vector<Move>& PV,
    bool leftMost,
    int ply);

Move findBestMove(
    Board &board,
    int numThreads,
    int maxDepth,
    int timeLimit,
    bool quiet
);
