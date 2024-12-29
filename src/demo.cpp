#include "chess.hpp"
#include "evaluation.hpp"
#include "search.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <tuple> 
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <unordered_map>

using namespace chess;

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
    // Board board1 = Board("r4rk1/pp3ppp/2pp1q2/2P1p3/N1PnP3/P4N1P/2P2PP1/R2Q1RK1 b - - 0 14");
    // std::cout << "Eval = " << evaluate(board1) << std::endl;

    Board board = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    // Tactical fen 
    //Board board = Board("2rq1rk1/pp3ppp/2p2n2/3p3P/3P1n2/2N2N2/PPPQ1PP1/1K1R3R b - - 2 16");
    std::vector<std::string> pgnMoves; // Store moves in PGN format
    // Default settings
    int depth = 20;
    int quiescenceDepth = 8;
    int numThreads = 8;
    int timeLimit = 30000;

    Move bestMove;

    int moveCount = 40;
    
    for (int i = 0; i < moveCount; i++) {
        // Start timer
        auto start = std::chrono::high_resolution_clock::now();

        Move bestMove = findBestMove(board, numThreads, depth, quiescenceDepth, timeLimit, true);

        // End timer
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        std::cout << "(Time taken: " << duration.count() << "s" << "; NPS: " << positionCount / duration.count() << ")"<< std::endl;

        if (bestMove == Move::NO_MOVE) {
            auto gameResult = board.isGameOver();
            if (gameResult.first == GameResultReason::CHECKMATE) {
                pgnMoves.push_back(board.sideToMove() == Color::WHITE ? "0-1" : "1-0");
            } else {
                pgnMoves.push_back("1/2-1/2");
            }
            break;
        }
        
        board.makeMove(bestMove);
        std::cout << "Move " << i + 1 << ": " << uci::moveToUci(bestMove) << std::endl;
        
        // std::string moveStr = uci::moveToUci(bestMove);
        
        // if (board.sideToMove() == Color::BLACK) {
        //     pgnMoves.push_back(std::to_string((i / 2) + 1) + ". " + moveStr);
        // } else {
        //     pgnMoves.back() += " " + moveStr;
        // }
    }

    // Write PGN to file
    writePNGToFile(pgnMoves, "game.pgn");
    std::cout << "Game saved to game.pgn" << std::endl;

    return 0;
}
