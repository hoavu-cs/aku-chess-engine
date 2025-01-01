#include "search.hpp"
#include "chess.hpp"
#include "evaluation.hpp"
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

std::vector<std::vector<Move>> killerMoves(100); // Killer moves
uint64_t positionCount = 0; // Number of positions evaluated for benchmarking

const size_t transpositionTableLimit = 1000000000; 
const int R = 2; 
const int razorMargin = 350; 

int nullDepthThreshold = 6; 
int razorPlyThreshold = 6; 

int globalMaxDepth = -1; // Maximum depth of current search
int globalQuiescenceDepth = 0; // Quiescence depth
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

// Transposition table lookup
bool probeTranspositionTable(std::map<std::uint64_t, 
                            std::pair<int, int>>& table, 
                            std::uint64_t hash, 
                            int depth, 
                            int& eval) {
    auto it = table.find(hash);
    return it != table.end() && it->second.second >= depth && (eval = it->second.first, true);
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

// Late move reduction
int depthReduction(Board& board, Move move, int i, int depth) {
    //return depth - 1;
    if (i <= 5) {
        return depth - 1;
    }  else {
        return depth / 2;
    }
}

// Generate a prioritized list of moves based on their tactical value
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
    std::vector<std::pair<Move, int>> candidateMoves;

    for (const auto& move : moves) {
        //int mateThreat = kingThreat(board, board.sideToMove());
        //if (mateThreat < 100) { 
            if (!board.isCapture(move) && !isPromotion(move)) {
                continue;
            }
        //}

        if (isPromotion(move)) {
            candidateMoves.push_back({move, 5000});
            continue;
        } else if (board.isCapture(move)) {
            auto victim = board.at<Piece>(move.to());
            auto attacker = board.at<Piece>(move.from());

            int priority = pieceValues[static_cast<int>(victim.type())] - pieceValues[static_cast<int>(attacker.type())];
            candidateMoves.push_back({move, priority});
        } 
        
        //else {
        //    candidateMoves.push_back({move, 0});
        //}
    }

    std::sort(candidateMoves.begin(), candidateMoves.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });
    
    for (const auto& [move, priority] : candidateMoves) {

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
                int quiescenceDepth,
                std::vector<Move>& PV) {

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

    if (depth <= 0) {
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

    // Null move pruning
    if (depth >= nullDepthThreshold) {
        if (!board.inCheck()) {
            board.makeNullMove();
            std::vector<Move> nullPV;
            int nullEval = alphaBeta(board, 
                                    depth - 1 - R,  
                                    alpha, 
                                    beta, 
                                    quiescenceDepth,
                                    nullPV);
            board.unmakeNullMove();

            if (whiteTurn && nullEval >= beta) {
                return beta;
            } else if (!whiteTurn && nullEval <= alpha) {
                return alpha;
            }
        }
    }

    // Razor pruning
    // int ply = globalMaxDepth - depth;
    // if (ply >= razorPlyThreshold && !board.inCheck()) {
    //     int staticEval = evaluate(board);
    //     if (whiteTurn && staticEval + razorMargin <= alpha) {
    //         return staticEval;
    //     } else if (!whiteTurn && staticEval - razorMargin >= beta) {
    //         return staticEval;
    //     }
    // }

    std::vector<std::pair<Move, int>> moves = generatePrioritizedMoves(board, depth);
    
    int bestEval = whiteTurn ? -INF : INF;

    for (int i = 0; i < moves.size(); i++) {
        Move move = moves[i].first;

        // Apply Late Move Reduction (LMR)
        int nextDepth = depthReduction(board, move, i, depth);

        board.makeMove(move);
        std::vector<Move> pvChild;
        int eval = alphaBeta(board, nextDepth, alpha, beta, quiescenceDepth, pvChild);
        board.unmakeMove(move);

        if (whiteTurn) {

            if (eval > alpha) {
                PV.clear();
                PV.push_back(move);
                for (auto& move : pvChild) {
                    PV.push_back(move);
                }
            }

            bestEval = std::max(bestEval, eval);
            alpha = std::max(alpha, eval);
        } else {

            if (eval < beta) {
                PV.clear();
                PV.push_back(move);
                for (auto& move : pvChild) {
                    PV.push_back(move);
                }
            }

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
    std::vector<Move> globalPV (maxDepth);

    globalDebug = debug;
    globalQuiescenceDepth = quiescenceDepth;

    // Set the number of threads
    omp_set_num_threads(numThreads);

    // Clear transposition tables
    #pragma omp critical
    {
        if (lowerBoundTable.size() > transpositionTableLimit) {
            lowerBoundTable.clear();
        }
        if (upperBoundTable.size() > transpositionTableLimit) {
            upperBoundTable.clear();
        }  
    }

    bool timeLimitExceeded = false;

    for (int depth = 1; depth <= maxDepth; ++depth) {
        globalMaxDepth = depth;
        nullDepthThreshold = depth / 2;
        razorPlyThreshold = depth - 2;
        
        // Track the best move for the current depth
        Move currentBestMove = Move();
        int currentBestEval = whiteTurn ? -INF : INF;

        std::vector<std::pair<Move, int>> newMoves;
        std::vector<Move> PV; // Principal variation

        // Generate all moves
        if (depth == 1) {
            moves = generatePrioritizedMoves(board, depth);
        }

        #pragma omp parallel
        {


            #pragma omp for
            for (int i = 0; i < moves.size(); i++) {

                if (timeLimitExceeded) {
                    continue; // Skip iteration if time limit is exceeded
                }

                Move move = moves[i].first;
                std::vector<Move> childPV; 
                
                Board localBoard = board;
                localBoard.makeMove(move);
                int nextDepth = depthReduction(localBoard, move, i, depth);
                int eval = alphaBeta(localBoard, nextDepth, -INF, INF, quiescenceDepth, childPV);
                localBoard.unmakeMove(move);

                #pragma omp critical
                newMoves.push_back({move, eval});

                #pragma omp critical
                {
                    if ((whiteTurn && eval > currentBestEval) || 
                        (!whiteTurn && eval < currentBestEval)) {
                        currentBestEval = eval;
                        currentBestMove = move;

                        PV.clear();
                        PV.push_back(move);
                        for (auto& move : childPV) {
                            PV.push_back(move);
                        }
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
            std::cout << "PV: ";
            
            for (const auto& move : PV) {
                std::cout << uci::moveToUci(move) << " ";
            }

            std::cout << std::endl;
        }

        moves = newMoves;

        // Check time limit again
        auto currentTime = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - startTime).count() >= timeLimit) {
            break; // Exit iterative deepening if time is up
        }
    }

    return bestMove; 
}
