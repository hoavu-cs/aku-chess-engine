/*
 * Author: Hoa T. Vu
 * Created: December 1, 2024
 * 
 * Copyright (c) 2024 Hoa T. Vu
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "chess.hpp"
#include "evaluation.hpp"
#include "search.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <chrono>

using namespace chess;
auto startTime = std::chrono::high_resolution_clock::now();

// Engine Metadata
const std::string ENGINE_NAME = "PIG ENGINE";
const std::string ENGINE_AUTHOR = "Hoa T. Vu";
bool resetHistory = false;

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

/*
    * Processes the "setoption" command and updates the engine options.
    * @param command The full setoption command received from the GUI.
*/

// void processSetOption(const std::string& command) {
//     std::istringstream iss(command);
//     std::string token, optionName, value;

//     iss >> token; // Skip "setoption"
//     iss >> token; // Skip "name"
//     std::getline(iss, optionName, ' ');

//     size_t pos = optionName.find(" value ");
//     if (pos != std::string::npos) {
//         value = optionName.substr(pos + 7);
//         optionName = optionName.substr(0, pos);
//     }

//     if (optionName == "Hash") {
//         int hashSize = std::stoi(value);
//         // Set hash table size
//     } else if (optionName == "Threads") {
//         int threads = std::stoi(value);
//         // Set number of threads
//     } else if (optionName == "Ponder") {
//         bool ponder = (value == "true");
//         // Enable or disable pondering
//     } else {
//         std::cerr << "Unknown option: " << optionName << std::endl;
//     }
// }


/**
 * Processes the "go" command and finds the best move.
 */
void processGo() {

    // Default settings
    int depth = 8;
    int quiescenceDepth = 10;
    int numThreads = 6;
    int timeLimit = 15000;
  
    // Simply find the best move without considering `t` or other options
    Move bestMove = Move::NO_MOVE;
    bestMove = findBestMove(board, numThreads, depth, quiescenceDepth, timeLimit, false, resetHistory, false);
    resetHistory = false;

    if (bestMove != Move::NO_MOVE) {
        std::cout << "bestmove " << uci::moveToUci(bestMove)  << std::endl;
    } else {
        std::cout << "bestmove 0000" << std::endl; // No legal moves
    }
}

/**
 * Handles the "uci" command and sends engine information.
 */
void processUci() {
    std::cout << "Engine's name: " << ENGINE_NAME << std::endl;
    std::cout << "Author:" << ENGINE_AUTHOR << std::endl;
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
            resetHistory = true;
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
