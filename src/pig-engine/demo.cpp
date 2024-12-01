#include "../../include/chess.hpp"
#include "../../include/evaluation_utils.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <tuple> 
#include <string>
#include <vector>
#include <algorithm>

using namespace chess;

long long positionCount = 0;
const int INF = 100000;
const int quiescenceDepth = 8;
const int normalDepth = 6;

// Transposition table type: maps Zobrist hash to a tuple (evaluation, depth)
std::map<std::uint64_t, std::tuple<int, int>> transpositionTable;

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
            priority = 1000 + (pieceValues[static_cast<int>(victim.type())] - pieceValues[static_cast<int>(attacker.type())]);
        } else {
            board.makeMove(move);
            if (board.inCheck()) { 
                // Assign a lower priority for moves that give check
                priority = 500;
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
        bool isCapture = board.isCapture(move), inCheck = board.inCheck();
        board.makeMove(move);
        board.unmakeMove(move);

        if (!isCapture && !inCheck) {
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
            return whiteTurn ? -INF : INF; // High positive/negative value based on player
        }
        return 0; // For stalemates or draws, return 0
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

            if (beta <= alpha) break; // Beta cutoff
        }
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

            if (beta <= alpha) break; // Alpha cutoff
        }
        return minEval;
    }
}

void writePNGToFile(const std::vector<std::string>& pgnMoves, std::string filename) {
    std::ofstream pgnFile("game.pgn");
    if (pgnFile.is_open()) {
        pgnFile << "[Event \"AI vs AI\"]\n";
        pgnFile << "[Site \"Local\"]\n";
        pgnFile << "[Date \"2024.11.29\"]\n";
        pgnFile << "[Round \"1\"]\n";
        pgnFile << "[White \"AI\"]\n";
        pgnFile << "[Black \"AI\"]\n";
        pgnFile << "[Result \"" << (pgnMoves.back().find("1-0") != std::string::npos
                                      ? "1-0"
                                      : pgnMoves.back().find("0-1") != std::string::npos
                                            ? "0-1"
                                            : "1/2-1/2") << "\"]\n\n";

        for (const auto& move : pgnMoves) {
            pgnFile << move << " ";
        }
        pgnFile << "\n";
        pgnFile.close();
    }
}



int main() {
    Board board = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    std::vector<std::string> pgnMoves; // Store moves in PGN format
    int depth = 6;
    int moveCount = 40;
    

    for (int i = 0; i < moveCount; i++) {
        Movelist moves;
        movegen::legalmoves(moves, board);

        if (moves.empty()) {
            auto gameResult = board.isGameOver();
            if (gameResult.first == GameResultReason::CHECKMATE) {
                pgnMoves.push_back(board.sideToMove() == Color::WHITE ? "0-1" : "1-0");
            } else {
                pgnMoves.push_back("1/2-1/2");
            }
            break;
        }

        bool whiteTurn = (board.sideToMove() == Color::WHITE);
        int bestEval = whiteTurn ? -INF : INF;
        
        Move bestMove = Move::NO_MOVE;
        
        std::vector<std::pair<Move, int>> moveCandidates = generatePrioritizedMoves(board);
        std::vector<std::pair<Move, int>> moveCandidates2 = generatePrioritizedMoves(board);

        for (int j = 0; j < moveCandidates.size(); j++) {
            const auto move = moveCandidates[j].first;
            board.makeMove(move);
            int eval = alphaBeta(board, normalDepth - 1, -INF, INF, !whiteTurn);
            board.unmakeMove(move);

            if ((whiteTurn && eval > bestEval) || (!whiteTurn && eval < bestEval)) {
                bestEval = eval;
                bestMove = move;
            }
        }

        std::cout << "Move: " << uci::moveToUci(bestMove) << " Eval: " << bestEval << std::endl;
        std::cout << "Position calculated: " << positionCount << std::endl;

        positionCount = 0;

        board.makeMove(bestMove);
        std::string moveStr = uci::moveToUci(bestMove);
        if (board.sideToMove() == Color::BLACK) {
            pgnMoves.push_back(std::to_string((i / 2) + 1) + ". " + moveStr);
        } else {
            pgnMoves.back() += " " + moveStr;
        }
    }

    // Write PGN to file
    writePNGToFile(pgnMoves, "game.pgn");
    std::cout << "Game saved to game.pgn" << std::endl;

    return 0;
}
