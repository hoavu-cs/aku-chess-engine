#include "chess.hpp"
#include "evaluation_utils.hpp"
#include "search.hpp"
#include <iostream>
#include <sstream>
#include <string>

using namespace chess;

// Engine Metadata
const std::string ENGINE_NAME = "MyChessEngine";
const std::string ENGINE_AUTHOR = "Your Name";

// Global Board State
Board board;

/**
 * Parses the "position" command and updates the board state.
 * @param command The full position command received from the GUI.
 */
void processPosition(const std::string& command) {
    std::istringstream iss(command);
    std::string token;

    iss >> token; // Skip "position"
    iss >> token; // "startpos" or "fen"

    if (token == "startpos") {
        board = Board(); // Initialize to the starting position
        if (iss >> token && token == "moves") {
            while (iss >> token) {
                Move move = uci::uciToMove(board, token);
                board.makeMove(move);
            }
        }
    } else if (token == "fen") {
        std::string fen;
        while (iss >> token && token != "moves") {
            if (!fen.empty()) fen += " ";
            fen += token;
        }
        board = Board(fen); // Set board to the FEN position
        if (token == "moves") {
            while (iss >> token) {
                Move move = uci::uciToMove(board, token);
                board.makeMove(move);
            }
        }
    }
}

/**
 * Processes the "go" command and finds the best move.
 */
void processGo() {
    Move bestMove = findBestMove(board); // Use your findBestMove implementation
    if (bestMove != Move::NO_MOVE) {
        std::cout << "bestmove " << uci::moveToUci(bestMove) << std::endl;
    } else {
        std::cout << "bestmove 0000" << std::endl; // No legal moves
    }
}

/**
 * Handles the "uci" command and sends engine information.
 */
void processUci() {
    std::cout << "PIG ENGINE " << ENGINE_NAME << std::endl;
    std::cout << "HOA T. Vu" << ENGINE_AUTHOR << std::endl;
    std::cout << "uciok" << std::endl;
}

/**
 * Main UCI loop to process commands from the GUI.
 */
void uciLoop() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "uci") {
            processUci();
        } else if (line == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (line == "ucinewgame") {
            board = Board(); // Reset board to starting position
        } else if (line.find("position") == 0) {
            processPosition(line);
        } else if (line.find("go") == 0) {
            processGo();
        } else if (line == "quit") {
            break;
        }
    }
}

/**
 * Entry point of the engine.
 */
int main() {
    uciLoop();
    return 0;
}
