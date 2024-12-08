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
#include <limits>
#include <mutex>
#include <omp.h> // Include OpenMP header

std::mutex mtx;

using namespace chess;

std::map<std::uint64_t, std::pair<int, int>> lowerBoundTable; // Hash -> (eval, depth)
std::map<std::uint64_t, std::pair<int, int>> upperBoundTable; // Hash -> (eval, depth)
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

int alphaBeta(chess::Board& board, int depth, int alpha, int beta, bool whiteTurn) {
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

    std::vector<std::pair<Move, int>> moveCandidates = generatePrioritizedMoves(board);

    if (whiteTurn) {
        int maxEval = -INF; // Large negative value
        for (int i = 0; i < moveCandidates.size(); i++) {
            const auto move = moveCandidates[i].first;

            board.makeMove(move); // Apply the move
            int eval = alphaBeta(board, depth - 1, alpha, beta, false);
            board.unmakeMove(move); // Revert the move

            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);

            if (beta <= alpha) {
                break; // Beta cutoff
            }
        }
        std::lock_guard<std::mutex> lock(mtx); // Lock the mutex
        lowerBoundTable[hash] = {maxEval, depth}; // Store lower bound
        return maxEval;

    } else {
        int minEval = INF; // Large positive value
        for (int i = 0; i < moveCandidates.size(); i++) {
            const auto move = moveCandidates[i].first;

            board.makeMove(move); // Apply the move
            int eval = alphaBeta(board, depth - 1, alpha, beta, true);
            board.unmakeMove(move); // Revert the move

            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);

            if (beta <= alpha) {
                break; // Alpha cutoff
            }
        }
        std::lock_guard<std::mutex> lock(mtx); // Lock the mutex
        upperBoundTable[hash] = {minEval, depth}; // Store upper bound
        return minEval;
    }
}

Move findBestMove(Board& board, int timeLimit = 60000) {
    using Clock = std::chrono::high_resolution_clock;
    auto startTime = Clock::now();

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
    std::string fen = board.getFen();

    int bestEval = whiteTurn ? -INF : INF;
    
    std::vector<std::pair<Move, int>> moveCandidates = generatePrioritizedMoves(board);
    Move bestMove = moveCandidates[0].first;

    int depth = 0;
    if (countPieces(board) <= 10) {
        depth = normalDepthEndgame; 
    } else {
        depth = normalDepth;
    }


    #pragma omp parallel for
    for (int j = 0; j < moveCandidates.size(); j++) {
        const auto move = moveCandidates[j].first;
        Board localBoard = board; // Thread-local copy of the board
        localBoard.makeMove(move);
        int eval = alphaBeta(localBoard, depth - 1, -INF, INF, !whiteTurn);
        localBoard.unmakeMove(move);

        #pragma omp critical
        {
            if ((whiteTurn && eval > bestEval) || (!whiteTurn && eval < bestEval)) {
                bestEval = eval;
                bestMove = move;
            }
        }

        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime).count();
        if (elapsedTime > timeLimit) {
            break; // Cannot break the loop in OpenMP directly; handle time externally if needed.
        }
    }

    if (lowerBoundTable.size() > maxTranspositionTableSize) {
        lowerBoundTable.clear();
    }
    if (upperBoundTable.size() > maxTranspositionTableSize) {
        upperBoundTable.clear();
    }

    return bestMove;
}

// void analyzeMoves(const std::vector<std::pair<Move, int>>& moves, Board& board, int depth, bool whiteTurn, 
//                   int& bestEval, Move& bestMove) {
//     int localBestEval = whiteTurn ? -INF : INF;
//     Move localBestMove = Move::NO_MOVE;

//     for (const auto& movePair : moves) {
//         const auto move = movePair.first;
        
//         board.makeMove(move);
//         int eval = alphaBeta(board, depth - 1, -std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), !whiteTurn);
//         board.unmakeMove(move);

//         if ((whiteTurn && eval > localBestEval) || (!whiteTurn && eval < localBestEval)) {
//             localBestEval = eval;
//             localBestMove = move;
//         }
//     }

//     bestEval = localBestEval;
//     bestMove = localBestMove;
// }


// Move findBestMove(Board& board, int timeLimit = 60000) {
//     using Clock = std::chrono::high_resolution_clock;
//     auto startTime = Clock::now();

//     Movelist moves;
//     movegen::legalmoves(moves, board);

//     if (moves.empty()) {
//         auto gameResult = board.isGameOver();
//         return Move::NO_MOVE;
//     }

//     bool whiteTurn = (board.sideToMove() == Color::WHITE);
//     int depth = countPieces(board) <= 10 ? normalDepthEndgame : normalDepth;

//     std::vector<std::pair<Move, int>> moveCandidates = generatePrioritizedMoves(board);

//     // Divide the moves into four roughly equal chunks
//     size_t chunkSize = (moveCandidates.size() + 3) / 4; // Ensure we handle remainders
//     std::vector<std::pair<Move, int>> chunk1(moveCandidates.begin(), moveCandidates.begin() + std::min(chunkSize, moveCandidates.size()));
//     std::vector<std::pair<Move, int>> chunk2(moveCandidates.begin() + chunkSize, moveCandidates.begin() + std::min(2 * chunkSize, moveCandidates.size()));
//     std::vector<std::pair<Move, int>> chunk3(moveCandidates.begin() + 2 * chunkSize, moveCandidates.begin() + std::min(3 * chunkSize, moveCandidates.size()));
//     std::vector<std::pair<Move, int>> chunk4(moveCandidates.begin() + 3 * chunkSize, moveCandidates.end());

//     // Variables to store the best results from each thread
//     int bestEval1 = whiteTurn ? -INF : INF;
//     Move bestMove1 = Move::NO_MOVE;

//     int bestEval2 = whiteTurn ? -INF : INF;
//     Move bestMove2 = Move::NO_MOVE;

//     int bestEval3 = whiteTurn ? -INF : INF;
//     Move bestMove3 = Move::NO_MOVE;

//     int bestEval4 = whiteTurn ? -INF : INF;
//     Move bestMove4 = Move::NO_MOVE;

//     Board board1 = board;
//     Board board2 = board;
//     Board board3 = board;
//     Board board4 = board;

//     // Launch four threads
//     std::thread t1(analyzeMoves, chunk1, std::ref(board1), depth, whiteTurn, std::ref(bestEval1), std::ref(bestMove1));
//     std::thread t2(analyzeMoves, chunk2, std::ref(board2), depth, whiteTurn, std::ref(bestEval2), std::ref(bestMove2));
//     std::thread t3(analyzeMoves, chunk3, std::ref(board3), depth, whiteTurn, std::ref(bestEval3), std::ref(bestMove3));
//     std::thread t4(analyzeMoves, chunk4, std::ref(board4), depth, whiteTurn, std::ref(bestEval4), std::ref(bestMove4));

//     // Wait for threads to finish
//     t1.join();
//     t2.join();
//     t3.join();
//     t4.join();

//     // Compare results from all threads
//     Move bestMove = bestMove1;
//     int bestEval = bestEval1;

//     if ((whiteTurn && bestEval2 > bestEval) || (!whiteTurn && bestEval2 < bestEval)) {
//         bestEval = bestEval2;
//         bestMove = bestMove2;
//     }
//     if ((whiteTurn && bestEval3 > bestEval) || (!whiteTurn && bestEval3 < bestEval)) {
//         bestEval = bestEval3;
//         bestMove = bestMove3;
//     }
//     if ((whiteTurn && bestEval4 > bestEval) || (!whiteTurn && bestEval4 < bestEval)) {
//         bestEval = bestEval4;
//         bestMove = bestMove4;
//     }

//     if (lowerBoundTable.size() > maxTranspositionTableSize) {
//         lowerBoundTable.clear();
//     }
//     if (upperBoundTable.size() > maxTranspositionTableSize) {
//         upperBoundTable.clear();
//     }

//     return bestMove;
// }
