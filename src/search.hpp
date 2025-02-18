#pragma once

#include "chess.hpp"

using namespace chess;

// Constants
const int INF = 100000;

// Function Declarations

std::vector<std::pair<Move, int>> generatePrioritizedMoves(Board &board,
                                                           int depth);

int alphaBeta(
    Board &board,
    int depth,
    int alpha,
    int beta,
    std::vector<Move> &pV,
    bool leftMost,
    int extension, 
    int ply);

Move findBestMove(
    Board &board,
    int numThreads,
    int maxDepth,
    int timeLimit,
    bool quiet);

Move parallelFindBestMove(
    Board &board, 
    int numThreads, 
    int maxDepth, 
    int timeLimit, 
    bool quiet);