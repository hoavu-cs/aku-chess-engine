#pragma once

#include "chess.hpp"

using namespace chess;

// Constants
const int INF = 100000;
extern std::uint64_t positionCount;

// Function Declarations

std::vector<std::pair<Move, int>> generatePrioritizedMoves(Board& board, 
                                                            int depth);

int quiescence(
    Board& board, 
    int depth, 
    int alpha, 
    int beta);

int alphaBeta(
    Board& board, 
    int depth, 
    int alpha, 
    int beta, 
    int quiescenceDepth,
    std::vector<Move>& pV,
    bool leftMost,
    int extension);

Move findBestMove(
    Board& board, 
    int numThreads, 
    int maxDepth, 
    int quiescenceDepth,
    int timeLimit,
    bool quiet);