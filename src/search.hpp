#pragma once

#include "chess.hpp"

// Constants
const int INF = 100000;
const int maxTranspositionTableSize = 100000000;

// Function Declarations

/**
 * Generates a prioritized list of moves based on their tactical value.
 * @param board The chess board for which to generate moves.
 * @return A vector of move-priority pairs sorted by priority.
 */
std::vector<std::pair<chess::Move, int>> generatePrioritizedMoves(chess::Board& board);

/**
 * Performs quiescence search to evaluate a position.
 * @param board The current chess board state.
 * @param depth The remaining depth for the quiescence search.
 * @param alpha The alpha bound for alpha-beta pruning.
 * @param beta The beta bound for alpha-beta pruning.
 * @return The evaluation score of the position.
 */
int quiescence(
    chess::Board& board, 
    int depth, 
    int alpha, 
    int beta
);

/**
 * Performs alpha-beta search to find the best move evaluation.
 * @param board The current chess board state.
 * @param depth The remaining depth for the search.
 * @param alpha The alpha bound for alpha-beta pruning.
 * @param beta The beta bound for alpha-beta pruning.
 * @param quiescenceDepth The depth for quiescence search.
 * @return The evaluation score of the position.
 */
int alphaBeta(
    chess::Board& board, 
    int depth, 
    int alpha, 
    int beta, 
    int quiescenceDepth
);

// /**
//  * @brief Evaluates the root position of the chess board up to a given depth.
//  * This function is mostly used mostly for a parallelized search at the second level of the search tree
//  * using OpenMP for multi-threading since the first level only has a few moves to evaluate.
//  * @param board Reference to the chess board object representing the current position.
//  * @param numThreads The number of threads to utilize for parallel search.
//  * @param depth The search depth limit for evaluation.
//  * @param quiescenceDepth The depth for quiescence search to resolve tactical sequences.
//  * @return The evaluation score of the root position after searching the second level.
//  */
// int evalSecondLevel(chess::Board& board, 
//                     int numThreads, 
//                     int depth, 
//                     int quiescenceDepth);

/**
 * Finds the best move for the current position using alpha-beta pruning.
 * @param board The current chess board state.
 * @param timeLimit The time limit for the search.
 * @param numThreads The number of threads to use for the search.
 * @param depth The normal search depth.
 * @param quiescenceDepth The depth for quiescence search.
 * @return The best move for the current position.
 */
chess::Move findBestMove(
    chess::Board& board, 
    int timeLimit, 
    int numThreads, 
    int depth, 
    int quiescenceDepth
);
