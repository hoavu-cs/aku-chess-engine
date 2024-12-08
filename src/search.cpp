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

std::map<std::uint64_t, std::pair<int, int>> lowerBoundTable; // Hash -> (eval, depth)
std::map<std::uint64_t, std::pair<int, int>> upperBoundTable; // Hash -> (eval, depth)

// Global variable to count the number of positions visited
long long positionCount = 0;

// Transposition table for white. At a node, look up the lower bound value for the current position.
bool probeLowerBoundTable(std::uint64_t hash, int depth, int& eval) {
    auto it = lowerBoundTable.find(hash);
    if (it != lowerBoundTable.end() && it->second.second >= depth) {
        eval = it->second.first;
        return true;
    }
    return false;
}

// Transposition table for black. At a node, look up the upper bound value for the current position.
bool probeUpperBoundTable(std::uint64_t hash, int depth, int& eval) {
    auto it = upperBoundTable.find(hash);
    if (it != upperBoundTable.end() && it->second.second >= depth) {
        eval = it->second.first;
        return true;
    }
    return false;
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

    // MVV-LVA values for each piece type
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

        if (board.isCapture(move)) { 
            // Calculate MVV-LVA priority for captures
            auto victim = board.at<Piece>(move.to());
            auto attacker = board.at<Piece>(move.from());
            priority = 300 + (pieceValues[static_cast<int>(victim.type())] - pieceValues[static_cast<int>(attacker.type())]);
        } else if (isPromotion(move)) {
            priority = 400;
        } else if (board.at<Piece>(move.from()).type() == PieceType::PAWN && board.at<Piece>(move.to()).type() == PieceType::PAWN) {
                priority = 200; // Pawn push
        } else {
            board.makeMove(move);
            if (board.inCheck()) { 
                priority = 100; // Check
            }
            board.unmakeMove(move);
        } 
        moveCandidates.push_back({move, priority});
    }

    // Sort moves by priority (descending order)
    std::sort(moveCandidates.begin(), moveCandidates.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    return moveCandidates;
}

int quiescence(chess::Board& board, int depth, int alpha, int beta, bool whiteTurn) {
    #pragma omp atomic
    positionCount++;

    // Safeguard: terminate if the depth limit is reached
    if (depth == 0) {
        return evaluate(board);
    }

    // Stand-pat evaluation: Evaluate the static position
    int standPat = evaluate(board);

    if (whiteTurn) {
        // Fail-hard beta cutoff
        if (standPat >= beta) {
            return beta;
        }
        // Update alpha if stand-pat improves the score
        if (standPat > alpha) {
            alpha = standPat;
        }
    } else {
        // Fail-hard alpha cutoff
        if (standPat <= alpha) {
            return alpha;
        }
        // Update beta if stand-pat improves the score
        if (standPat < beta) {
            beta = standPat;
        }
    }

    // Generate capture moves only
    Movelist moves;
    movegen::legalmoves(moves, board);

    // Evaluate each capture move
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
            // Beta cutoff for maximizing player
            if (score >= beta) {
                return beta;
            }
            // Update alpha if score improves it
            if (score > alpha) {
                alpha = score;
            }
        } else {
            // Alpha cutoff for minimizing player
            if (score <= alpha) {
                return alpha;
            }
            // Update beta if score improves it
            if (score < beta) {
                beta = score;
            }
        }
    }

    // Return alpha for maximizing player or beta for minimizing player
    return whiteTurn ? alpha : beta;
}

int alphaBeta(chess::Board& board, 
                int depth, 
                int alpha, 
                int beta, 
                bool whiteTurn, 
                int quiescenceDepth, 
                int R) {
    #pragma omp atomic
    positionCount++;

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

    if (whiteTurn) {
        if (probeLowerBoundTable(hash, depth, storedEval) && storedEval >= beta) {
            return storedEval; // Beta cutoff from lower bound
        }
    } else {
        if (probeUpperBoundTable(hash, depth, storedEval) && storedEval <= alpha) {
            return storedEval; // Alpha cutoff from upper bound
        }
    }

    // Base case: if depth is zero, evaluate the position
    if (depth == 0) {
        return quiescence(board, quiescenceDepth, alpha, beta, whiteTurn);
    }

    // null move heuristics
    if (depth > R) {
        if (whiteTurn && !board.inCheck()) {
            board.makeNullMove();
            int nullEval = alphaBeta(board, depth - R - 1, alpha, beta, false, quiescenceDepth, R);
            board.unmakeNullMove();
            if (nullEval >= beta) {
                return beta;
            }
        } else if (!whiteTurn && !board.inCheck()) {
            board.makeNullMove();
            int nullEval = alphaBeta(board, depth - R - 1, alpha, beta, true, quiescenceDepth, R);
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

            board.makeMove(move); // Apply the move
            int eval = alphaBeta(board, depth - 1, alpha, beta, false, quiescenceDepth, R);
            board.unmakeMove(move); // Revert the move

            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);

            if (beta <= alpha) {
                break; // Beta cutoff
            }
        }
        #pragma omp critical
        lowerBoundTable[hash] = {maxEval, depth}; // Store lower bound
        return maxEval;

    } else {
        int minEval = INF; // Large positive value
        for (int i = 0; i < moveCandidates.size(); i++) {
            const auto move = moveCandidates[i].first;

            board.makeMove(move); // Apply the move
            int eval = alphaBeta(board, depth - 1, alpha, beta, true, quiescenceDepth, R);
            board.unmakeMove(move); // Revert the move

            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);

            if (beta <= alpha) {
                break; // Alpha cutoff
            }
        }
        #pragma omp critical
        upperBoundTable[hash] = {minEval, depth}; // Store upper bound
        return minEval;
    }
}

Move findBestMove(Board& board, 
                int timeLimit = 60000, 
                int numThreads = 4, 
                int normalDepth = 6, 
                int quiescenceDepth = 10,
                int normalDepthEndgame = 8,
                int R = 3) {

    if (board.fullMoveNumber() > 15) {
        normalDepth = 4;
        quiescenceDepth = 12;
    }

    omp_set_num_threads(numThreads);
    // using Clock = std::chrono::high_resolution_clock;
    // auto startTime = Clock::now();

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

    // If the game is in the endgame, increase the search depth
    int depth = (countPieces(board) <= 10) ? normalDepthEndgame : normalDepth;

    #pragma omp parallel for
    for (int j = 0; j < moveCandidates.size(); j++) {
        const auto move = moveCandidates[j].first;
        Board localBoard = board; // Thread-local copy of the board
        localBoard.makeMove(move);
        int eval = alphaBeta(localBoard, depth - 1, -INF, INF, !whiteTurn, quiescenceDepth, R);
        localBoard.unmakeMove(move);

        #pragma omp critical
        {
            if ((whiteTurn && eval > bestEval) || (!whiteTurn && eval < bestEval)) {
                bestEval = eval;
                bestMove = move;
            }
        }
        //auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime).count();
    }

    if (lowerBoundTable.size() > maxTranspositionTableSize) {
        lowerBoundTable.clear();
    }
    if (upperBoundTable.size() > maxTranspositionTableSize) {
        upperBoundTable.clear();
    }

    return bestMove;
}
