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

// Engine Metadata
const std::string ENGINE_NAME = "PIG ENGINE";
const std::string ENGINE_AUTHOR = "Hoa T. Vu";

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
void processGo(const std::vector<std::string>& tokens) {

    // Default settings
    int depth = 30;
    int quiescenceDepth = 8;
    int numThreads = 6;
    int timeLimit = 15000; // Default to 15 seconds
    bool quiet = false;
  
    // Simply find the best move without considering `t` or other options
    Move bestMove = Move::NO_MOVE;

    /*--------------------------------------------------------------
    Time control:
    Option 1: movetime <x>
    Option 2: wtime <x> btime <x> winc <x> binc <x> movestogo <x>
    ---------------------------------------------------------------*/

    int wtime = 0, btime = 0, winc = 0, binc = 0, movestogo = 0, movetime = 0;
    for (size_t i = 1; i < tokens.size(); ++i) {
        if (tokens[i] == "wtime" && i + 1 < tokens.size()) {
            wtime = std::stoi(tokens[i + 1]); // Remaining time for White
        } else if (tokens[i] == "btime" && i + 1 < tokens.size()) {
            btime = std::stoi(tokens[i + 1]); // Remaining time for Black
        } else if (tokens[i] == "winc" && i + 1 < tokens.size()) {
            winc = std::stoi(tokens[i + 1]); // Increment for White
        } else if (tokens[i] == "binc" && i + 1 < tokens.size()) {
            binc = std::stoi(tokens[i + 1]); // Increment for Black
        } else if (tokens[i] == "movestogo" && i + 1 < tokens.size()) {
            movestogo = std::stoi(tokens[i + 1]); // Moves remaining
        } else if (tokens[i] == "movetime" && i + 1 < tokens.size()) {
            movetime = std::stoi(tokens[i + 1]); // Time per move
        }
    }


    if (movetime > 0) {
        timeLimit = movetime * 0.6;
    } else {
        // Determine the time limit based on the current player's time and increment
        if (board.sideToMove() == Color::WHITE && wtime > 0) {
            int baseTime = wtime / (movestogo > 0 ? movestogo + 1 : 40); 
            timeLimit = static_cast<int>(baseTime * 0.6) + winc;
        } else if (board.sideToMove() == Color::BLACK && btime > 0) {
            int baseTime = btime / (movestogo > 0 ? movestogo + 1 : 40); 
            timeLimit = static_cast<int>(baseTime * 0.6) + binc;
        }
    }

    bestMove = findBestMove(board, numThreads, depth, quiescenceDepth, timeLimit, quiet);

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
        } else if (line.find("position") == 0) {
            processPosition(line);
        } else if (line.find("go") == 0) {
            std::vector<std::string> tokens;
            std::istringstream iss(line);
            std::string token;
            while (iss >> token) {
                tokens.push_back(token);
            }
            processGo(tokens);
        } else if (line == "quit") {
            break;
        }
    }
}

int main() {
    uciLoop();
    return 0;
}
