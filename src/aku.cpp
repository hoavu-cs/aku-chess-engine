/*
* Author: Hoa T. Vu
* Created: December 1, 2024
* 
* Copyright (c) 2024 Hoa T. Vu
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to use,
* copy, modify, merge, publish, and distribute copies of the Software for 
* **non-commercial purposes only**, provided that the following conditions are met:
* 
* 1. The above copyright notice and this permission notice shall be included in
*    all copies or substantial portions of the Software.
* 2. Any use of this Software for commercial purposes **requires prior written
*    permission from the author, Hoa T. Vu**.
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
#include "openings.hpp"
#include "search.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdio>  
#include <stdexcept> 
#include "eg_table_inc.hpp"

using namespace chess;

// Engine Metadata
const std::string ENGINE_NAME = "Aku Chess Engine";
const std::string ENGINE_AUTHOR = "Hoa T. Vu";


/*-------------------------------------------------------------------------------------------- 
    Initialize endgame tablebases.
--------------------------------------------------------------------------------------------*/

// Get the executable's directory path
#ifdef _WIN32
    #include <windows.h>
#elif __APPLE__
    #include <mach-o/dyld.h>
#elif __linux__
    #include <unistd.h>
#endif


std::string getExecutablePath() {
    char path[1024];

#ifdef _WIN32
    GetModuleFileNameA(nullptr, path, sizeof(path));
#elif __APPLE__
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) {
        throw std::runtime_error("Buffer too small"); // Use std::runtime_error instead
    }
#elif __linux__
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (count == -1) {
        std::cerr << "Failed to get executable path" << std::endl;    }
    path[count] = '\0';  // Null-terminate the string
#else
    throw std::filesystem::runtime_error("Unsupported OS");
#endif

    return std::filesystem::canonical(std::filesystem::path(path)).parent_path().string();
}

// Extract tablebase files to the current directory if they don't already exist.
void extractTablebaseFiles() {

    std::string path = getExecutablePath();
    std::filesystem::path tablesDir = std::filesystem::path(path) / "tables";

    // Check if the "tables" folder exists, if not, create it
    if (!std::filesystem::exists(tablesDir)) {
        std::cout << "Creating directory: " << tablesDir << std::endl;
        if (!std::filesystem::create_directories(tablesDir)) {
            std::cerr << "Failed to create directory: " << tablesDir << std::endl;
            return;
        }
    }

    for (size_t i = 0; i < tablebaseFileCount; i++) {
        std::string filePath = path + "/" + tablebaseFiles[i].name;

        // Check if the file already exists
        if (std::filesystem::exists(filePath)) {
            //std::cout << "info skipping: " << filePath << " (already exists)" << std::endl;
            continue;
        }

        // Create and write file only if it doesn't exist
        std::ofstream outFile(filePath, std::ios::binary);
        if (!outFile) {
            std::cerr << "info failed to create: " << filePath << std::endl;
            continue;
        }

        outFile.write(reinterpret_cast<const char*>(tablebaseFiles[i].data), tablebaseFiles[i].size);
        outFile.close();
        std::cout << "info extracted: " << filePath << std::endl;
    }
}


/*-------------------------------------------------------------------------------------------- 
    Global variables
-------------------------------------------------------------------------------------------- */
int numThreads = 8;
int depth = 30;

std::string getBookMove(Board& board) {
    std::vector<std::string> possibleMoves;
    std::srand(std::time(0)); // Seed random number generator

    for (const auto& sequence : OPENING_MOVES) {
        Board tempBoard;
        bool match = true;
        
        // Consider the first move if the board is in the starting position
        if (board.getFen() == Board().getFen() && !sequence.empty()) {
            possibleMoves.push_back(sequence[0]);
            continue;
        }
        
        for (size_t i = 0; i < sequence.size(); ++i) {
            try {
                Move moveObj = uci::uciToMove(tempBoard, sequence[i]);
                tempBoard.makeMove(moveObj);
            } catch (const std::exception& e) {
                match = false;
                break;
            }
            
            // If the board matches at any prefix of the sequence, consider the next move
            if (tempBoard.getFen() == board.getFen() && i + 1 < sequence.size()) {
                possibleMoves.push_back(sequence[i + 1]);
            }
        }
    }

    if (!possibleMoves.empty()) {
        return possibleMoves[std::rand() % possibleMoves.size()]; // Return a random move
    }
    return ""; // No match found
}


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

    board = Board(); // Initialize to the starting position

    if (token == "startpos") {
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

/*--------------------------------------------------------------------------------
    * Processes the "setoption" command and updates the engine options.
    * @param command The full setoption command received from the GUI.   
---------------------------------------------------------------------------------*/

void processSetOption(const std::vector<std::string>& tokens) {

    std::string optionName = tokens[2];
    std::string value = tokens[4];


    if (optionName == "Threads") {
        numThreads = std::stoi(value);
        // Set number of threads
    } else if (optionName == "Depth") {
        depth = std::stoi(value);
    } else {
        std::cerr << "Unknown option: " << optionName << std::endl;
    }
}


/**
 * Processes the "go" command and finds the best move.
 */
void processGo(const std::vector<std::string>& tokens) {

    // Default settings
    
    int timeLimit = 30000; // Default to 15 seconds
    bool quiet = false;

    // Simply find the best move without considering `t` or other options
    Move bestMove = Move::NO_MOVE;

    // Opening book
    std::string bookMove = getBookMove(board);
    if (!bookMove.empty()) {
        Move moveObj = uci::uciToMove(board, bookMove);
        board.makeMove(moveObj);
        std::cout << "info depth 0 score cp 0 nodes 0 time 0 pv " << bookMove << std::endl;
        std::cout << "bestmove " << bookMove << std::endl;
        return;
    }

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

    double adjust = 0.6;
    if (movetime > 0) {
        timeLimit = movetime * adjust;
    } else {
        // Determine the time limit based on the current player's time and increment
        if (board.sideToMove() == Color::WHITE && wtime > 0) {
            int baseTime = wtime / (movestogo > 0 ? movestogo + 2 : 40); 
            timeLimit = static_cast<int>(baseTime * adjust) + winc;
        } else if (board.sideToMove() == Color::BLACK && btime > 0) {
            int baseTime = btime / (movestogo > 0 ? movestogo + 2 : 40); 
            timeLimit = static_cast<int>(baseTime * adjust) + binc;
        }
    }
    bestMove = findBestMove(board, numThreads, depth, timeLimit, quiet);

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
    std::cout << "option name Threads type spin default 8 min 1 max 8" << std::endl;
    std::cout << "option name Depth type spin default 99 min 1 max 99" << std::endl;
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
        } else if (line.find("setoption") == 0) {
            //std::cout << "set option being processed" << std::endl;
            std::vector<std::string> tokens;
            std::istringstream iss(line);
            std::string token;
            while (iss >> token) {
                tokens.push_back(token);
            }
            processSetOption(tokens);
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
    
    initializeNNUE();

    std::string path = getExecutablePath() + "/tables/";
    extractTablebaseFiles();
    initializeTB(path);

    uciLoop();

    return 0;
}