#include "search.hpp"
#include "chess.hpp"
#include "evaluation.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <tuple> 
#include <string>
#include <vector>
#include <algorithm>
#include <omp.h> // Include OpenMP header
#include <chrono>
#include <stdlib.h>

using namespace chess;

// Constants and global variables
std::map<std::uint64_t, std::pair<int, int>> lowerBoundTable; // Hash -> (eval, depth)
std::map<std::uint64_t, std::pair<int, int>> upperBoundTable; // Hash -> (eval, depth)
int globalMaxDepth = 0; // Maximum depth
bool globalDebug = false; // Debug flag

// Basic piece values for move ordering
const int pieceValues[] = {
    0,    // No piece
    100,  // Pawn
    320,  // Knight
    330,  // Bishop
    500,  // Rook
    900,  // Queen
    20000 // King
};

std::vector<std::vector<Move>> killerMoves(100); // Killer moves

uint64_t positionCount = 0; // Number of positions evaluated for benchmarking
const size_t maxTableSize = 1000000000; // Maximum size of the transposition table

const int nullMoveThreshold = 3; // Null move pruning threshold
const int R = 2; // Null move reduction
const int razorMargin = 200; // Razor pruning margin

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
    #pragma omp critical
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

    // Move ordering 1. promotion 2. captures 3. killer moves 4. check moves

    for (const auto& move : moves) {
        int priority = 0;

        if (isPromotion(move)) {

            priority = 5000; 

        } else if (board.isCapture(move)) { 

            // Calculate MVV-LVA priority for captures
            auto victim = board.at<Piece>(move.to());
            auto attacker = board.at<Piece>(move.from());

            priority = 4000 + pieceValues[static_cast<int>(victim.type())] - pieceValues[static_cast<int>(attacker.type())];
        
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

int quiescence(Board& board, int depth, int alpha, int beta) {
    
    if (globalDebug) {
        #pragma  omp critical
        positionCount++;
    }
    
    if (depth == 0) {
        return evaluate(board);
    }

    // Stand-pat evaluation: Evaluate the static position
    int standPat = evaluate(board);
    bool whiteTurn = board.sideToMove() == Color::WHITE;
 
    // perform stand-pat pruning only if not in check
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
    std::vector<std::pair<Move, int>> captureMoves;

    for (const auto& move : moves) {

        
        // Ignore non-captures, non-promotions, and non-checks if not in check
        // If in check, keep searching
        if (!board.isCapture(move) && !isPromotion(move)) {
            continue;
        }
        
        auto attacker = board.at<Piece>(move.from());
        auto victim = board.at<Piece>(move.to());

        int priority = pieceValues[static_cast<int>(victim.type())] - pieceValues[static_cast<int>(attacker.type())];
        captureMoves.push_back({move, priority});
    }

    std::sort(captureMoves.begin(), captureMoves.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });
    
    for (const auto& [move, priority] : captureMoves) {

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

int alphaBeta(Board& board, 
                   int depth, 
                   int alpha, 
                   int beta, 
                   int quiescenceDepth) {

    if (globalDebug) {
        #pragma  omp critical
        positionCount++;
    }

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
    if (globalMaxDepth - depth  >= 6) {
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
            int nullEval = alphaBeta(board, 
                                        depth - 1 - R,  
                                        alpha, 
                                        beta, 
                                        quiescenceDepth);
            board.unmakeNullMove();

            if (whiteTurn && nullEval >= beta) {
                return beta;
            } else if (!whiteTurn && nullEval <= alpha) {
                return alpha;
            }
        }
    }

    std::vector<std::pair<Move, int>> moves = generatePrioritizedMoves(board, depth);
    
    int bestEval = whiteTurn ? -INF : INF;

    for (int i = 0; i < moves.size(); i++) {
        Move move = moves[i].first;

        // Apply Late Move Reduction (LMR)
        int newDepth = depth - 1;
        if (!board.inCheck() && i >= 7 && depth >= 6) {
            newDepth -= 1;
        }

        board.makeMove(move);
        int eval = alphaBeta(board, newDepth, alpha, beta, quiescenceDepth);
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

    return bestEval;
}


Move findBestMove(Board& board, 
                  int numThreads = 4, 
                  int maxDepth = 6, 
                  int quiescenceDepth = 10, 
                  int timeLimit = 5000,
                  bool debug = false) {
    // Initialize variables
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime = 
        std::chrono::high_resolution_clock::now();
    Move bestMove = Move(); 
    int bestEval = (board.sideToMove() == Color::WHITE) ? -INF : INF;
    bool whiteTurn = board.sideToMove() == Color::WHITE;
    std::vector<std::pair<Move, int>> moves;
    globalMaxDepth = maxDepth;
    globalDebug = debug;

    // Set the number of threads
    omp_set_num_threads(numThreads);

    // Clear transposition tables
    #pragma omp critical
    {
        lowerBoundTable.clear();
        upperBoundTable.clear();
    }

    bool timeLimitExceeded = false;

    // Iterative deepening loop
    for (int depth = 1; depth <= maxDepth; ++depth) {
        // Track the best move for the current depth
        Move currentBestMove = Move();
        int currentBestEval = whiteTurn ? -INF : INF;
        std::vector<std::pair<Move, int>> newMoves;

        // Generate all moves
        if (depth == 1) {
            moves = generatePrioritizedMoves(board, depth);
        }

        #pragma omp parallel
        {
            std::vector<std::pair<Move, int>> localNewMoves; // Thread-local storage

            #pragma omp for
            for (int i = 0; i < moves.size(); i++) {
                if (timeLimitExceeded) {
                    continue; // Skip iteration if time limit is exceeded
                }

                Move move = moves[i].first;
                // Apply Late Move Reduction (LMR)
                int newDepth = depth - 1;
                if (!board.inCheck() && i >= 6 && depth >= 6) {
                    newDepth -= 1;
                }

                Board localBoard = board;
                localBoard.makeMove(move);
                int eval = alphaBeta(localBoard, newDepth, -INF, INF, quiescenceDepth);
                localBoard.unmakeMove(move);

                localNewMoves.push_back({move, eval});

                #pragma omp critical
                {
                    if ((whiteTurn && eval > currentBestEval) || 
                        (!whiteTurn && eval < currentBestEval)) {
                        currentBestEval = eval;
                        currentBestMove = move;
                    }
                }

                // Check time limit
                auto currentTime = std::chrono::high_resolution_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(
                        currentTime - startTime).count() >= timeLimit) {
                    #pragma omp critical
                    {
                        timeLimitExceeded = true;
                    }
                }
            }

            // Merge thread-local results into the global list
            #pragma omp critical
            {
                newMoves.insert(newMoves.end(), localNewMoves.begin(), localNewMoves.end());
            }
        }

        // Update the global best move and evaluation after this depth
        bestMove = currentBestMove;
        bestEval = currentBestEval;

        if (whiteTurn) {
            std::sort(newMoves.begin(), newMoves.end(), [](const auto& a, const auto& b) {
                return a.second > b.second;
            });
        } else {
            std::sort(newMoves.begin(), newMoves.end(), [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
        }

        if (debug) {
            std::cout << "---------------------------------" << std::endl;
            for (int j = 0; j < std::min<int>(5, newMoves.size()); j++) {
                std::cout << "Depth: " << depth 
                        << " Move: " << uci::moveToUci(newMoves[j].first) 
                        << " Eval: " << newMoves[j].second << std::endl;
            }
        }

        moves = newMoves;

        // Check time limit again
        auto currentTime = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - startTime).count() >= timeLimit) {
            break; // Exit iterative deepening if time is up
        }
    }

    return bestMove; // Return the best move found
}
