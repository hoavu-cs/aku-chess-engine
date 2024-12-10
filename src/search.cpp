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
#include <chrono>
#include <random>
#include <omp.h> // Include OpenMP header
#include <stdlib.h>

using namespace chess;


// Constants and global variables
std::map<std::uint64_t, std::pair<int, int>> lowerBoundTable; // Hash -> (eval, depth)
std::map<std::uint64_t, std::pair<int, int>> upperBoundTable; // Hash -> (eval, depth)
long long positionCount = 0;
const int shallowDepth = 5;
const int nullMoveDepth = 4;
const long long unsigned int numShallowMoves = 3;
const size_t maxTableSize = 100000000;

// Transposition table for white. At a node, look up the lower bound value for the current position.
bool probeTranspositionTable(std::map<std::uint64_t, std::pair<int, int>>& table, std::uint64_t hash, int depth, int& eval) {
    auto it = table.find(hash);
    return it != table.end() && it->second.second >= depth && (eval = it->second.first, true);
}

// Clear the transposition tables if they exceed a certain size
void clearTranspositionTables(size_t maxSize) {
    if (lowerBoundTable.size() > maxSize) lowerBoundTable.clear();
    if (upperBoundTable.size() > maxSize) upperBoundTable.clear();
}

// Transposition table type: maps Zobrist hash to a tuple (evaluation, depth)
std::map<std::uint64_t, std::tuple<int, int>> transpositionTable;

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

    for (int j = 0; j < moves.size(); j++) {
        const auto move = moves[j];
        int priority = 0;

        if (isPromotion(move)) {
            priority = 5000; 
        } else if (board.isCapture(move)) { 
            // Calculate MVV-LVA priority for captures
            auto victim = board.at<Piece>(move.to());
            auto attacker = board.at<Piece>(move.from());
            priority = 4000 + (pieceValues[static_cast<int>(victim.type())] - pieceValues[static_cast<int>(attacker.type())]);
        } else {
            bool isCheck = false;
            board.makeMove(move);
            isCheck = board.inCheck();
            board.unmakeMove(move);

            if (isCheck) {
                priority = 3000;
            } 
        } 

        moveCandidates.push_back({move, priority});
    }

    std::sort(moveCandidates.begin(), moveCandidates.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });
    return moveCandidates;
}

int quiescence(chess::Board& board, int depth, int alpha, int beta, bool whiteTurn) {
    #pragma omp atomic
    positionCount++;
    if (depth == 0) {
        return evaluate(board);
    }

    // Stand-pat evaluation: Evaluate the static position
    int standPat = evaluate(board);

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
        bool isCapture = board.isCapture(move), inCheck = board.inCheck(), isPromo = isPromotion(move);
        board.makeMove(move);
        board.unmakeMove(move);

        if (!isCapture && !inCheck && !isPromo) {
            continue;
        }

        int score;
        board.makeMove(move);
        if (whiteTurn) {
            // Maximizing player searches for the highest score
            score = quiescence(board, depth - 1, alpha, beta, false);
        } else {
            // Minimizing player searches for the lowest score
            score = quiescence(board, depth - 1, alpha, beta, true);
        }
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

int alphaBeta(chess::Board& board, 
                int depth, 
                int alpha, 
                int beta, 
                bool whiteTurn, 
                int quiescenceDepth) {

    // Check if the game is over
    auto gameOverResult = board.isGameOver();
    if (gameOverResult.first != GameResultReason::NONE) {
        // If the game is over, return an appropriate evaluation
        if (gameOverResult.first == GameResultReason::CHECKMATE) {
            if (whiteTurn) {
                return -INF - board.halfMoveClock(); // Get the fastest checkmate possible
            } else {
                return INF + board.halfMoveClock(); 
            }
        }
        return 0; // For stalemates or draws, return 0
    }

    // Probe the transposition table
    std::uint64_t hash = board.hash();
    int storedEval;

    if ((whiteTurn && probeTranspositionTable(lowerBoundTable, hash, depth, storedEval) && storedEval >= beta) ||
        (!whiteTurn && probeTranspositionTable(upperBoundTable, hash, depth, storedEval) && storedEval <= alpha)) {
        return storedEval;
    }

    // Base case: if depth is zero, evaluate the position using quiescence search
    if (depth == 0) {
        return quiescence(board, quiescenceDepth, alpha, beta, whiteTurn);
    }

    // null move heuristics
    if (depth > nullMoveDepth) {
        if (whiteTurn && !board.inCheck()) {
            board.makeNullMove();
            int nullEval = alphaBeta(board, nullMoveDepth, alpha, beta, false, quiescenceDepth);
            board.unmakeNullMove();
            if (nullEval >= beta) {
                return beta;
            }
        } else if (!whiteTurn && !board.inCheck()) {
            board.makeNullMove();
            int nullEval = alphaBeta(board, nullMoveDepth, alpha, beta, true, quiescenceDepth);
            board.unmakeNullMove();
            if (nullEval <= alpha) {
                return alpha;
            }
        }
    }

    std::vector<std::pair<Move, int>> moveCandidates = generatePrioritizedMoves(board);

    if (whiteTurn) {
        int maxEval = -INF; // Large negative value

        for (int i = 0; i < moveCandidates.size(); i++) {
            const auto move = moveCandidates[i].first;

            board.makeMove(move); 
            int eval = alphaBeta(board, depth - 1, alpha, beta, false, quiescenceDepth);
            board.unmakeMove(move); 

            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);

            if (beta <= alpha) {
                break; 
            }
        }
        #pragma omp critical
        lowerBoundTable[hash] = {maxEval, depth}; // Store lower bound
        return maxEval;

    } else {
        int minEval = INF; // Large positive value

        for (int i = 0; i < moveCandidates.size(); i++) {
            const auto move = moveCandidates[i].first;

            board.makeMove(move); 
            int eval = alphaBeta(board, depth - 1, alpha, beta, true, quiescenceDepth);
            board.unmakeMove(move); 

            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);

            if (beta <= alpha) {
                break; 
            }
        }
        #pragma omp critical
        upperBoundTable[hash] = {minEval, depth}; // Store upper bound
        return minEval;
    }
}

// This function evaluates the root position up to the given depth.
// It is mostly use to utilize OpenMP for parallelization in the second level of the search tree.
int evalSecondLevel(Board& board, 
                int numThreads, 
                int depth, 
                int quiescenceDepth) {
    
    omp_set_num_threads(numThreads);

    Movelist moves;
    movegen::legalmoves(moves, board);

    if (moves.empty()) {
        auto gameResult = board.isGameOver();
        if (gameResult.first == GameResultReason::CHECKMATE) {
            return Move::NO_MOVE;
        } else {
            return Move::NO_MOVE;
        }
    }

    bool whiteTurn = (board.sideToMove() == Color::WHITE);
    int bestEval = whiteTurn ? -INF : INF;
    
    std::vector<std::pair<Move, int>> moveCandidates = generatePrioritizedMoves(board);
    Move bestMove = moveCandidates[0].first;


    #pragma omp parallel for
    for (int j = 0; j < std::min(moveCandidates.size(), numShallowMoves); j++) {

        const auto move = moveCandidates[j].first;
        Board localBoard = board; // Thread-local copy of the board
        localBoard.makeMove(move);
        int eval = alphaBeta(localBoard, depth - 1, -INF, INF, !whiteTurn, quiescenceDepth);
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

    clearTranspositionTables(maxTableSize);

    return bestEval;
}


Move findBestMove(Board& board, 
                int timeLimit = 60000, 
                int numThreads = 4, 
                int depth = 6, 
                int quiescenceDepth = 10) {

    omp_set_num_threads(numThreads);

    Movelist moves;
    movegen::legalmoves(moves, board);

    if (moves.empty()) {
        auto gameResult = board.isGameOver();
        if (gameResult.first == GameResultReason::CHECKMATE) {
            return Move::NO_MOVE;
        } else {
            return Move::NO_MOVE;
        }
    }

    bool whiteTurn = (board.sideToMove() == Color::WHITE);
    int bestEval = whiteTurn ? -INF : INF;
    
    std::vector<std::pair<Move, int>> moveCandidates = generatePrioritizedMoves(board);
    std::vector<std::pair<Move, int>> shallowedMoves;
    Move bestMove = moveCandidates[0].first;

    // Do a shallow search to find a good ordering of moves
    for (int i = 0; i < moveCandidates.size(); i++) {
        const auto move = moveCandidates[i].first;
        int eval = 0;

        board.makeMove(move);
        if (whiteTurn) {
            eval = alphaBeta(board, shallowDepth, -INF, INF, false, quiescenceDepth);
        } else {
            eval = alphaBeta(board, shallowDepth, -INF, INF, true, quiescenceDepth);
        }
        board.unmakeMove(move);

        shallowedMoves.push_back({move, eval});
    }

    // Sort the moves by the shallow evaluation
    if (whiteTurn) {
        std::sort(shallowedMoves.begin(), shallowedMoves.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });
    } else {
        std::sort(shallowedMoves.begin(), shallowedMoves.end(), [](const auto& a, const auto& b) {
            return a.second < b.second;
        });
    }

    #pragma omp parallel for
    for (int j = 0; j < std::min(shallowedMoves.size(), numShallowMoves); j++) {

        const auto move = shallowedMoves[j].first;
        Board localBoard = board; // Thread-local copy of the board
        localBoard.makeMove(move);
        //int eval = alphaBeta(localBoard, depth - 1, -INF, INF, !whiteTurn, quiescenceDepth);
        int eval = evalSecondLevel(localBoard, numThreads, depth - 1, quiescenceDepth);
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

    clearTranspositionTables(maxTableSize);

    return bestMove;
}
