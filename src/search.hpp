#pragma once

#include "chess.hpp"

using namespace chess;

// Constants
const int INF = 100000;
const int maxTranspositionTableSize = 100000000;
extern std::uint64_t positionCount;

// Function Declarations

std::vector<std::pair<Move, int>> generatePrioritizedMoves(Board& board, 
                                                            int depth);

int quiescence(
    Board& board, 
    int depth, 
    int alpha, 
    int beta,
    int numChecks
);

int alphaBeta(
    Board& board, 
    int depth, 
    int alpha, 
    int beta, 
    int quiescenceDepth,
    std::vector<Move>& pV);

Move findBestMove(
    Board& board, 
    int numThreads, 
    int depth, 
    int quiescenceDepth,
    int timeLimit,
    bool debug,
    bool resetHistory);