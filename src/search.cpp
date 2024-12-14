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
uint64_t positionCount = 0;
const int nullMoveDepth = 4;
const size_t maxTableSize = 100000000;

// Transposition table lookup
bool probeTranspositionTable(std::map<std::uint64_t, std::pair<int, int>>& table, std::uint64_t hash, int depth, int& eval) {
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

// Horizon effect test fen 8/5k1p/2N5/3p2p1/P2Pn1P1/P2KP2P/8/8 w - - 7 51
std::vector<std::pair<Move, int>> generatePrioritizedMoves(Board& board) {
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

    for (const auto& move : moves) {
        int priority = 0;

        if (isPromotion(move)) {
            priority = 5000; 
        } else if (board.isCapture(move)) { 
            // Calculate MVV-LVA priority for captures
            auto victim = board.at<Piece>(move.to());
            auto attacker = board.at<Piece>(move.from());
            priority = 4000 + pieceValues[static_cast<int>(attacker.type())] - pieceValues[static_cast<int>(victim.type())];
        } else {
            board.makeMove(move);
            bool isCheck = board.inCheck();
            board.unmakeMove(move);
            if (isCheck) {
                priority = 3000;
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
    //bool inCheck = board.inCheck();
    for (const auto& move : moves) {

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

int alphaBeta(Board& board, 
                int depth, 
                int alpha, 
                int beta, 
                int quiescenceDepth) {

    #pragma  omp critical
    positionCount++;

    // Check if the game is over
    bool whiteTurn = board.sideToMove() == Color::WHITE;
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
        return quiescence(board, quiescenceDepth, alpha, beta);
    }

    // null move heuristics
    if (depth > nullMoveDepth) {
        if (whiteTurn && !board.inCheck()) {
            board.makeNullMove();
            int nullEval = alphaBeta(board, nullMoveDepth, alpha, beta, quiescenceDepth);
            board.unmakeNullMove();

            if (nullEval >= beta) {
                return beta;
            }
        } else if (!whiteTurn && !board.inCheck()) {
            board.makeNullMove();
            int nullEval = alphaBeta(board, nullMoveDepth, alpha, beta, quiescenceDepth);
            board.unmakeNullMove();

            if (nullEval <= alpha) {
                return alpha;
            }
        }
    }

    std::vector<std::pair<Move, int>> moveCandidates = generatePrioritizedMoves(board);

    if (whiteTurn) {
        int maxEval = -INF; // Large negative value

        for (auto& [move, priority] : moveCandidates) {
            board.makeMove(move); 
            int eval = alphaBeta(board, depth - 1, alpha, beta, quiescenceDepth);
            board.unmakeMove(move); 

            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);

            if (beta <= alpha) break; 
        }
        #pragma omp critical
        lowerBoundTable[hash] = {maxEval, depth}; // Store lower bound
        return maxEval;

    } else {
        int minEval = INF; // Large positive value

        for (auto& [move, priority] : moveCandidates) {
            board.makeMove(move); 
            int eval = alphaBeta(board, depth - 1, alpha, beta, quiescenceDepth);
            board.unmakeMove(move); 

            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);

            if (beta <= alpha) break; 
        }
        #pragma omp critical
        upperBoundTable[hash] = {minEval, depth}; // Store upper bound
        return minEval;
    }
}


std::vector<std::pair<Move, int>> getShallowCandidates(Board& board, int lookAheadDepth, int quiescenceDepth, int alpha, int beta, int k) {

    std::vector<std::pair<Move, int>> moveCandidates = generatePrioritizedMoves(board);
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

int alphaBetaPrune(Board& board, 
                   int depth, 
                   int lookAheadDepth, 
                   int k, 
                   int alpha, 
                   int beta, 
                   int quiescenceDepth) {

    #pragma  omp critical
    positionCount++;

    if (depth == 0) {
        return quiescence(board, quiescenceDepth, alpha, beta);
    }

    bool whiteTurn = board.sideToMove() == Color::WHITE;
    std::vector<std::pair<Move, int>> scoredMoves = getShallowCandidates(board, lookAheadDepth, quiescenceDepth, alpha, beta, k);
    int bestEval = whiteTurn ? -INF : INF;

    // Recurse on the top k moves with reduced depth
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

    if (lookAheadDepth >= depth) {
        return scoredMoves[0].first;
    }

    Move bestMove = Move::NO_MOVE;
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
        int eval = alphaBeta(localBoard, lookAheadDepth, -INF, INF, quiescenceDepth);
        localBoard.unmakeMove(move);

        #pragma omp critical 
        {
            if ((whiteTurn && eval > bestEval) || (!whiteTurn && eval < bestEval)) {
                bestEval = eval;
                bestMove = move;
            }
        }

        Square from = scoredMoves[i].first.from();
        if (board.at<Piece>(from).type() == PieceType::PAWN) { 
            // If a panw push is good enough, return it. This avoids infinite moves in the endgame.
            if (whiteTurn && eval > 600 || !whiteTurn && eval < -600) {
                #pragma omp critical
                {
                    bestPawnMove = move;
                }
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

    if (bestPawnMove.promotionType() == PieceType::QUEEN) {
        return bestPawnMove;
    } else if (bestPawnMove != Move::NO_MOVE) {
        return bestPawnMove;
    }

    clearTranspositionTables(maxTableSize); // Clear the transposition tables if they exceed a certain size

    return bestMove;
}
