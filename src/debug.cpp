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
#include <unordered_set>

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
    // https://www.wtharvey.com/anan.html
    
    Board board = Board("r2q1r1k/1b3p2/p2Ppn2/1p4Q1/8/3B4/PPP2PPP/R4RK1 w - - 1 22"); // tactical test
    // Tactical fen 
    //Board board = Board("2rq1rk1/pp3ppp/2p2n2/3p3P/3P1n2/2N2N2/PPPQ1PP1/1K1R3R b - - 2 16");
    std::vector<std::string> pgnMoves; // Store moves in PGN format
    //board = Board("2rq1rk1/pp3ppp/2p2n2/3p3P/3P1n2/2N2N2/PPPQ1PP1/1K1R3R b - - 2 16");

    // board = Board("rnbq1rk1/1pN2ppp/p3p3/2bp4/4n3/3BPNB1/PPP2PPP/R2QK2R b KQ - 1 10");
    // board = Board("4r1k1/1pq2ppp/p7/2Pp4/P1b1rR2/2P1P1Q1/6PP/R1B3K1 b - - 3 24");
    // board = Board("rnbq1rk1/1pN2ppp/p3p3/2bp4/4n3/3BPNB1/PPP2PPP/R2QK2R b KQ - 1 10");
    // board = Board("3rr1k1/1ppbqppp/p1nbpn2/3pN3/3P1P2/P1NQP1B1/1PP1B1PP/R4RK1 b - - 0 12");
    // board = Board("r1br4/2kp2pp/ppnRP3/8/P1B5/2N5/1P4PP/2R3K1 w - - 1 26");
    // board = Board("r3kbnr/pp1n1ppp/4p3/2ppP3/8/2N2N2/PPPP1PPP/R1B1K2R b KQkq - 1 8");
    // board = Board("1r1q1rk1/1ppb1pp1/1bn1p1np/p3P3/P1Bp2QP/1N1P1N2/1PP2PP1/R1B1R1K1 b - - 0 14");
    //board = Board("3r1rk1/1pqnbppp/p3p1n1/2p1P3/3pQ2P/N2P1N2/PPP2PP1/R1B1R1K1 b - - 0 14");
    // board = Board("rnbqkbnr/ppp1pppp/8/3P4/8/8/PPPP1PPP/RNBQKBNR b KQkq - 0 2");
    // board = Board("r3kb1r/ppp1qp2/2nnb2p/6p1/3N4/2NBB1Q1/PPP2PPP/R3K2R b KQkq - 1 12");
    // board = Board("r2q1rk1/p4ppp/2pb1n2/3p2B1/8/2QP1P2/PPP2P1P/RN3RK1 b - - 0 13");
    // board = Board("2qr2k1/1p2rppp/p2BPn2/5p2/3Q4/5P2/PP4PP/2R1R1K1 b - - 4 24");
    // board = Board("5rk1/pp4pp/2b1p3/2Pp2q1/P6n/2N1RP2/1PP2P1P/R2Q1K2 b - - 4 18");
    // board = Board("1rbqk2r/1p2b1pp/2p1p3/1B3p2/1n3Q2/3P1N2/PPP2PPP/R1B2RK1 b k - 1 16");
    // board = Board("1r2k2r/1pq1bppp/p3p3/2p1n3/3pNB2/1Q1P4/PPP2PPP/4RR1K b k - 5 16");
    // board = Board("8/4k1p1/6K1/p1PP1pPP/P4P2/8/8/8 w - - 1 60"); // tactical test
    //board = Board("r4rk1/1pp1qp2/1p2b3/1B2p1p1/4Pn1p/2Q2P2/PPP2BPP/R2R2K1 b - - 7 19");
    //board = Board("Q7/P4rk1/3q1np1/8/3p1b1p/1P1P3P/2P1R1P1/5R1K b - - 0 37"); // tactical test
    //board = Board("2k4r/1r1q2pp/QBp2p2/1p6/8/8/P4PPP/2R3K1 w - - 1 1"); // mate in 4.
    // board = Board("6k1/5p1p/4p1p1/2p1P3/2P4P/3P2PK/R1Q3B1/1r1n2q1 b - - 0 1"); // tactical test
    // board = Board("r2qkb1r/3bpp2/p1np1p2/1p3P2/3NP2p/2N5/PPPQB1PP/R4RK1 b kq - 0 1"); // tactical test
    // board = Board("3qbrk1/5p2/8/3pP1bQ/1PpB4/2P5/6PP/5RK1 w - - 0 1"); // mate in 6
    // board = Board("r1bqk2r/pp1n1pp1/2pBp3/8/4B2p/3R4/P3QPP1/3R2K1 w q - 0 28"); // tactical test
    // board = Board("8/2p2k1p/3p4/3P3q/1p4R1/P1B2P2/4r3/Q5K1 w - - 1 42"); // mate threat test
    // board = Board("r1b2rk1/pp1p1p2/5p1p/3P4/1n6/3B1N2/P4PPP/R3K2R w KQ - 1 18"); // 
    // board = Board("4b3/4bpk1/4p3/1p2P1P1/4NQ2/p5K1/3R4/6q1 w - - 2 46"); // promotion test
    // board = Board("r1bq1k1r/pp1p1p2/1n3n2/2p3B1/2PQ4/8/P4PPP/2K1RB1R w - - 0 17"); // tactical test
    // board = Board("rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2");
    //board = Board("8/4r1k1/2Pp1q2/1p1B3p/5PP1/1Q3K1P/8/8 w - - 1 47");
    //board = Board("8/6pk/3pp2p/4p1nP/1P2P3/3P1rP1/4qPK1/2QN3R b - - 0 1"); // mate puzzle
    // board = Board("5rk1/1p2qpp1/p2Qp1p1/2n1P3/2P5/5N2/P4PPP/3R2K1 b - - 2 24"); // mate blunder test
    // board = Board("5rk1/1p1bbp2/2p1p1p1/2PpP1Pp/1q1P3P/4PR2/1rB2Q2/R4NK1 w - - 0 32");
    //board = Board("r5k1/1p4pp/2p1b3/3pP3/pq1P2PQ/4PR2/8/5RK1 w - - 2 45"); // promotion test
    // board = Board("r3r1k1/pppb1ppp/1q2N3/3Pn3/2B1p3/P3P1P1/1P2QPP1/2RR2K1 w - - 3 23");
    // board = Board("8/8/8/2K5/8/8/5k2/6r1 w - - 0 1");
    //board = Board("8/8/3k4/8/8/8/3K4/4R3 w - - 0 1");

    // Default settings
    int depth = 30;
    int numThreads = 6;
    int timeLimit = 30000;

    Move bestMove;

    int moveCount = 40;

    for (int i = 0; i < moveCount; i++) {

        Move bestMove = findBestMove(board, numThreads, depth, timeLimit, true);

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
        
    }

    return 0;
}
