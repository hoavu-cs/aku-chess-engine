#include "search.hpp"
#include "chess.hpp"
#include "evaluation_utils.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <tuple> 
#include <string>
#include <vector>
#include <algorithm>
#include <omp.h> // Include OpenMP header
#include <stdlib.h>

using namespace chess;

// Constants and global variables
std::map<std::uint64_t, std::pair<int, int>> lowerBoundTable; // Hash -> (eval, depth)
std::map<std::uint64_t, std::pair<int, int>> upperBoundTable; // Hash -> (eval, depth)

std::vector<std::vector<Move>> killerMoves(100); // Killer moves

uint64_t positionCount = 0; // Number of positions evaluated for benchmarking

const int nullMoveThreshold = 3; // Null move pruning threshold
const int R = 2; // Null move reduction
const int razorMargin = 300; // Razor pruning margin

const size_t maxTableSize = 100000000; // Maximum size of the transposition table

// Transposition table lookup
bool probeTranspositionTable(std::map<std::uint64_t, 
                            std::pair<int, int>>& table, 
                            std::uint64_t hash, 
                            int depth, 
                            int& eval) {
    auto it = table.find(hash);
    return it != table.end() && it->second.second >= depth && (eval = it->second.first, true);
}

// Clear the transposition tables if they exceed a certain size
void clearTranspositionTables(size_t maxSize) {
    #pragma omp critical
    {
        if (lowerBoundTable.size() > maxSize) lowerBoundTable.clear();
        if (upperBoundTable.size() > maxSize) upperBoundTable.clear();
    }
}

// Transposition table type: maps Zobrist hash to a tuple (evaluation, depth)
std::map<std::uint64_t, std::tuple<int, int>> transpositionTable;

// Check if a move is a promotion
bool isPromotion(const Move& move) {
    return (move.typeOf() & Move::PROMOTION) != 0;
}

// Update the killer moves
void updateKillerMoves(const Move& move, int depth) {
    # pragma omp critical
    {
        if (killerMoves[depth].size() < 2) {
            killerMoves[depth].push_back(move);
        } else {
            killerMoves[depth][1] = killerMoves[depth][0];
            killerMoves[depth][0] = move;
        }
    }
}

std::vector<std::pair<Move, int>> generatePrioritizedMoves(Board& board, int depth) {
    Movelist moves;
    movegen::legalmoves(moves, board);
    std::vector<std::pair<Move, int>> moveCandidates;

    const int pieceValues[] = {
        0,    // No piece
        100,  // Pawn
        320,  // Knight
        330,  // Bishop
        500,  // Rook
        900,  // Queen
        20000 // King
    };

    // Move ordering 1. promotion 2. captures 3. killer moves 4. check moves

    for (const auto& move : moves) {
        int priority = 0;

        if (isPromotion(move)) {

            priority = 5000; 

        } else if (board.isCapture(move)) { 

            // Calculate MVV-LVA priority for captures
            auto victim = board.at<Piece>(move.to());
            auto attacker = board.at<Piece>(move.from());

            priority = 4000 + pieceValues[static_cast<int>(attacker.type())] - pieceValues[static_cast<int>(victim.type())];
        
        } else if (std::find(killerMoves[depth].begin(), killerMoves[depth].end(), move) != killerMoves[depth].end()) {
        
            priority = 3000;

        } else {

            board.makeMove(move);
            bool isCheck = board.inCheck();
            board.unmakeMove(move);

            if (isCheck) {
                priority = 2000;
            } 

        } 

        moveCandidates.push_back({move, priority});
    }

    // Sort the moves by priority
    std::sort(moveCandidates.begin(), moveCandidates.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    return moveCandidates;
}


std::vector<std::pair<Move, int>> getShallowCandidates(Board& board, 
                                                    int lookAheadDepth, 
                                                    int quiescenceDepth, 
                                                    int alpha, 
                                                    int beta, 
                                                    int k) {

    std::vector<std::pair<Move, int>> moveCandidates = generatePrioritizedMoves(board, lookAheadDepth);
    std::vector<std::pair<Move, int>> scoredMoves;

    for (auto& [move, priority] : moveCandidates) {

        board.makeMove(move);
        int eval = alphaBeta(board, lookAheadDepth, alpha, beta, quiescenceDepth);
        board.unmakeMove(move);
        scoredMoves.push_back({move, eval});

    }

    bool whiteTurn = board.sideToMove() == Color::WHITE;

    if (whiteTurn) {
        std::sort(scoredMoves.begin(), scoredMoves.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
    } else {
        std::sort(scoredMoves.begin(), scoredMoves.end(),
                  [](const auto& a, const auto& b) { return a.second < b.second; });
    }

    if (scoredMoves.size() > k) {
        scoredMoves = std::vector<std::pair<Move, int>>(scoredMoves.begin(), scoredMoves.begin() + k);
    }

    return scoredMoves;
}

int quiescence(Board& board, int depth, int alpha, int beta) {
    
    #pragma  omp critical
    positionCount++;
    
    if (depth == 0) {
        return evaluate(board);
    }

    // Stand-pat evaluation: Evaluate the static position
    int standPat = evaluate(board);
    bool whiteTurn = board.sideToMove() == Color::WHITE;

    if (whiteTurn) {
        if (standPat >= beta) {
            return beta;
        }
        if (standPat > alpha) {
            alpha = standPat;
        }
    } else {
        if (standPat <= alpha) {
            return alpha;
        }
        if (standPat < beta) {
            beta = standPat;
        }
    }

    Movelist moves;
    movegen::legalmoves(moves, board);

    // Evaluate each capture, check, or promotion
    
    for (const auto& move : moves) {
        bool isCheckMove = false;

        board.makeMove(move);
        if (board.inCheck()) {
            isCheckMove = true;
        }
        board.unmakeMove(move);

        if (!board.isCapture(move) && !isPromotion(move))
            continue;

        board.makeMove(move);
        int score = quiescence(board, depth - 1, alpha, beta);
        board.unmakeMove(move);

        if (whiteTurn) {
            if (score >= beta) {
                return beta;
            }
            if (score > alpha) {
                alpha = score;
            }
        } else {
            if (score <= alpha) {
                return alpha;
            }
            if (score < beta) {
                beta = score;
            }
        }
    }

    return whiteTurn ? alpha : beta;
}

// Base alpha-beta search function without any pruning.
// This is mostly used to generate candidates for deeper search.
int alphaBeta(Board& board, 
                int depth, 
                int alpha, 
                int beta, 
                int quiescenceDepth) {

    #pragma  omp critical
    positionCount++;

    bool whiteTurn = board.sideToMove() == Color::WHITE;

    // Check if the game is over
    auto gameOverResult = board.isGameOver();
    if (gameOverResult.first != GameResultReason::NONE) {
        if (gameOverResult.first == GameResultReason::CHECKMATE) {
            if (whiteTurn) {
                return -INF/2 + (1000 - depth); // Get the fastest checkmate possible
            } else {
                return INF/2 - (1000 - depth); 
            }
        }
        return 0; // For stalemates or draws, return 0
    }

    // Probe the transposition table
    std::uint64_t hash = board.hash();
    bool found = false;
    int storedEval;

    #pragma omp critical
    { 
        if ((whiteTurn && probeTranspositionTable(lowerBoundTable, hash, depth, storedEval) && storedEval >= beta) ||
            (!whiteTurn && probeTranspositionTable(upperBoundTable, hash, depth, storedEval) && storedEval <= alpha)) {
            found = true;
        }
    }

    if (found) {
        return storedEval;
    }

    // Base case: if depth is zero, evaluate the position using quiescence search
    if (depth == 0) {
        int quiescenceEval = quiescence(board, quiescenceDepth, alpha, beta);
        
        if (whiteTurn) {
            #pragma omp critical
            lowerBoundTable[hash] = {quiescenceEval, depth};
        } else {
            #pragma omp critical
            upperBoundTable[hash] = {quiescenceEval, depth};
        }

        return quiescenceEval;
    }

    std::vector<std::pair<Move, int>> moveCandidates = generatePrioritizedMoves(board, depth);
    int bestEval = whiteTurn ? -INF : INF;

    for (auto& [move, priority] : moveCandidates) {

        board.makeMove(move); 
        int eval = alphaBeta(board, depth - 1, alpha, beta, quiescenceDepth);
        board.unmakeMove(move); 

        if (whiteTurn) {
            bestEval = std::max(bestEval, eval);
            alpha = std::max(alpha, eval);
        } else {
            bestEval = std::min(bestEval, eval);
            beta = std::min(beta, eval);
        }

        if (beta <= alpha) {
            updateKillerMoves(move, depth);
            break; 
        }
    }

    if (whiteTurn) {
            #pragma omp critical
            lowerBoundTable[hash] = {bestEval, depth}; 
    } else {
            #pragma omp critical
            upperBoundTable[hash] = {bestEval, depth}; 
    }

    return bestEval;
}

int alphaBetaPrune(Board& board, 
                   int depth, 
                   int lookAheadDepth, 
                   int k, 
                   int alpha, 
                   int beta, 
                   int quiescenceDepth) {

    #pragma  omp critical
    positionCount++;

    bool whiteTurn = board.sideToMove() == Color::WHITE;

    // Check if the game is over
    auto gameOverResult = board.isGameOver();

    if (gameOverResult.first != GameResultReason::NONE) {
        if (gameOverResult.first == GameResultReason::CHECKMATE) {
            if (whiteTurn) {
                return -INF/2 + (1000 - depth); // Get the fastest checkmate possible
            } else {
                return INF/2 - (1000 - depth); 
            }
        }
        return 0;
    }

    // Probe the transposition table
    std::uint64_t hash = board.hash();
    bool found = false;
    int storedEval;

    #pragma omp critical
    { 
        if ((whiteTurn && probeTranspositionTable(lowerBoundTable, hash, depth, storedEval) && storedEval >= beta) ||
            (!whiteTurn && probeTranspositionTable(upperBoundTable, hash, depth, storedEval) && storedEval <= alpha)) {
            found = true;
        }
    }
    if (found) {
        return storedEval;
    } 

    if (depth == 0) {
        int quiescenceEval = quiescence(board, quiescenceDepth, alpha, beta);
        
        if (whiteTurn) {
            #pragma omp critical
            lowerBoundTable[hash] = {quiescenceEval, depth};
        } else {
            #pragma omp critical
            upperBoundTable[hash] = {quiescenceEval, depth};
        }
        
        return quiescenceEval;
    }

    // Razor pruning
    if (depth == 1) {
        int staticEval = evaluate(board);
        if (whiteTurn && staticEval + razorMargin <= alpha) {
            return staticEval;
        } else if (!whiteTurn && staticEval - razorMargin >= beta) {
            return staticEval;
        }
    }

    // Null move pruning
    if (depth >= nullMoveThreshold) {
        if (!board.inCheck()) {
            board.makeNullMove();
            int nullEval = alphaBetaPrune(board, depth - 1 - R, lookAheadDepth, k, alpha, beta, quiescenceDepth);
            board.unmakeNullMove();

            if (whiteTurn && nullEval >= beta) {
                return beta;
            } else if (!whiteTurn && nullEval <= alpha) {
                return alpha;
            }
        }
    }

    std::vector<std::pair<Move, int>> scoredMoves = 
        getShallowCandidates(board, lookAheadDepth, quiescenceDepth, alpha, beta, k);

    int bestEval = whiteTurn ? -INF : INF;

    for (auto& [move, score] : scoredMoves) {
        board.makeMove(move);
        int eval = alphaBetaPrune(board, depth - 1, lookAheadDepth, k, alpha, beta, quiescenceDepth);
        board.unmakeMove(move);

        if (whiteTurn) {
            bestEval = std::max(bestEval, eval);
            alpha = std::max(alpha, eval);
        } else {
            bestEval = std::min(bestEval, eval);
            beta = std::min(beta, eval);
        }

        if (beta <= alpha) {
            updateKillerMoves(move, depth);
            break;
        }
    }

    if (whiteTurn) {
            #pragma omp critical
            lowerBoundTable[board.hash()] = {bestEval, depth}; 
    } else {
            #pragma omp critical
            upperBoundTable[board.hash()] = {bestEval, depth}; 
    }

    clearTranspositionTables(maxTableSize); // Clear the transposition tables if they exceed a certain size

    return bestEval;
}

Move findBestMove(Board& board, 
                int numThreads = 4, 
                int depth = 6, 
                int lookAheadDepth = 4,
                int k = 10,
                int quiescenceDepth = 10) {

    #pragma omp critical
    positionCount = 0;

    Movelist moves;
    movegen::legalmoves(moves, board);

    if (moves.empty()) {
        return Move::NO_MOVE;
    }
    
    bool whiteTurn = board.sideToMove() == Color::WHITE;
    std::vector<std::pair<Move, int>> scoredMoves = getShallowCandidates(board, lookAheadDepth, quiescenceDepth, -INF, INF, k);
    
    // If lookAheadDepth is greater than or equal to the search depth, use vanilla alpha-beta search
    if (lookAheadDepth >= depth) { 
        return scoredMoves[0].first;
    }

    Move bestMove = scoredMoves[0].first;
    Move bestPawnMove = Move::NO_MOVE;
    int bestEval = whiteTurn ? -INF : INF;

    // Set the number of threads
    omp_set_num_threads(numThreads);

    // Evaluate each move using alpha-beta search with the shallower lookAheadDepth
    #pragma omp parallel for // Use OpenMP parallel for parallelism at the root level
    for (int i = 0; i < scoredMoves.size(); i++) { 

        auto [move, priority] = scoredMoves[i];
        Board localBoard = board; // Thread-local copy of the board

        localBoard.makeMove(move);
        int eval = alphaBetaPrune(localBoard, depth - 1, lookAheadDepth, k, -INF, INF, quiescenceDepth);
        localBoard.unmakeMove(move);

        #pragma omp critical 
        {
            if ((whiteTurn && eval > bestEval) || (!whiteTurn && eval < bestEval)) {
                bestEval = eval;
                bestMove = move;
            }
        }
    }

    if (whiteTurn) {
            #pragma omp critical
            lowerBoundTable[board.hash()] = {bestEval, depth}; 
    } else {
            #pragma omp critical
            upperBoundTable[board.hash()] = {bestEval, depth}; 
    }

    clearTranspositionTables(maxTableSize); // Clear the transposition tables if they exceed a certain size

    return bestMove;
}
