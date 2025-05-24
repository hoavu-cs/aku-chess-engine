/*
 * aku.cpp - Aku Chess Engine
 * 
 * Author: Hoa T. Vu
 * Created: December 1, 2024
 *
 * This file is part of the Aku Chess Engine.
 *
 * The Aku Chess Engine is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The Aku Chess Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
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
#include "assets.hpp"
#include "syzygy.hpp"

using namespace chess;

// Engine Metadata
const std::string ENGINE_NAME = "Aku Chess Engine";
const std::string ENGINE_AUTHOR = "Hoa T. Vu";

// Engine tunable parameters
int rfp_depth = 2;
int rfp_c1 = 200; // previously 250

int fp_depth = 2;
int fp_c1 = 200; // previously 250

int lmp_depth = 1;
int lmp_c1 = 13;

int hp_depth = 2;
int hp_c1 = 4000;

int singular_c1 = 2;
int singular_c2 = 0;

float lmr_1 = 0.75f;
float lmr_2 = 0.45f;


// Initialize Syzygy tablebases and NNUE weights.
#ifdef _WIN32
    #include <windows.h>
#elif __APPLE__
    #include <mach-o/dyld.h>
#elif __linux__
    #include <unistd.h>
#endif

std::string get_exec_path() {
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
void extract_files() {

    std::string path = get_exec_path();
    std::filesystem::path table_dir = std::filesystem::path(path) / "tables";

    // Check if the "tables" folder exists, if not, create it
    if (!std::filesystem::exists(table_dir)) {
        std::cout << "Creating directory: " << table_dir << std::endl;
        if (!std::filesystem::create_directories(table_dir)) {
            std::cerr << "Failed to create directory: " << table_dir << std::endl;
            return;
        }
    }

    for (size_t i = 0; i < tablebaseFileCount; i++) {
        std::string file_path = path + "/" + tablebaseFiles[i].name;

        // Check if the file already exists
        if (std::filesystem::exists(file_path)) {
            continue;
        }

        // Create and write file only if it doesn't exist
        std::ofstream out_file(file_path, std::ios::binary);
        if (!out_file) {
            std::cerr << "Failed to create: " << file_path << std::endl;
            continue;
        }

        out_file.write(reinterpret_cast<const char*>(tablebaseFiles[i].data), tablebaseFiles[i].size);
        out_file.close();
        std::cout << "Extracted: " << file_path << std::endl;
    }


    // Ensure "nnue" directory exists
    std::string nnue_dir = path + "/nnue";
    if (!std::filesystem::exists(nnue_dir)) {
        std::cout << "Creating directory: " << nnue_dir << std::endl;
        if (!std::filesystem::create_directories(nnue_dir)) {
            std::cerr << "Failed to create directory: " << nnue_dir << std::endl;
            return;
        }
    }

    // Extract NNUE weights file
    std::string nnue_file_path = nnue_dir + "/" + nnueWeightFile.name;
    std::ofstream nnue_out(nnue_file_path, std::ios::binary);
    if (!nnue_out) {
        std::cerr << "Failed to create: " << nnue_file_path << std::endl;
    } else {
        nnue_out.write(reinterpret_cast<const char*>(nnueWeightFile.data), nnueWeightFile.size);
        nnue_out.close();
        std::cout << "Extracted: " << nnue_file_path << std::endl;
    }
}

// Global variables for engine options
int num_threads = 4;
int depth = 50;
bool chess960 = false;
bool internal_opening = true;
Board board;

std::string get_book_move(Board& board) {
    std::vector<std::string> possible_moves;
    std::srand(std::time(0)); // Seed random number generator

    for (const auto& sequence : OPENING_MOVES) {
        Board temp_board;
        bool match = true;
        
        // Consider the first move if the board is in the starting position
        if (board.getFen() == Board().getFen() && !sequence.empty()) {
            possible_moves.push_back(sequence[0]);
            continue;
        }
        
        for (size_t i = 0; i < sequence.size(); ++i) {
            try {
                Move moveObj = uci::uciToMove(temp_board, sequence[i]);
                temp_board.makeMove(moveObj);
            } catch (const std::exception& e) {
                match = false;
                break;
            }
            
            // If the board matches at any prefix of the sequence, consider the next move
            if (temp_board.getFen() == board.getFen() && i + 1 < sequence.size()) {
                possible_moves.push_back(sequence[i + 1]);
            }
        }
    }

    if (!possible_moves.empty()) {
        return possible_moves[std::rand() % possible_moves.size()]; // Return a random move
    }
    return ""; // No match found
}

// Parses the "position" command and updates the board state.
void process_position(const std::string& command) {

    std::istringstream iss(command);
    std::string token;

    iss >> token; // Skip "position"
    iss >> token; // "startpos" or "fen"

    if (token == "startpos") {
        board = Board(); // Initialize to the starting position
        board.set960(chess960); // Set chess960 if applicable    

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
        board.set960(chess960); // Set chess960 if applicable

        if (token == "moves") {
            while (iss >> token) {
                Move move = uci::uciToMove(board, token);
                board.makeMove(move);
            }
        }
    }
}

// Processes the "setoption" command and updates the engine options.
void process_option(const std::vector<std::string>& tokens) {

    std::string option_name = tokens[2];
    std::string value = tokens[4];

    if (option_name == "Threads") {
        num_threads = std::stoi(value);
        // Set number of threads
    } else if (option_name == "Depth") {
        depth = std::stoi(value);
    } else if (option_name == "Hash") {
        table_size = std::stoi(value) * 1024 * 1024 / 64;
    } else if (option_name == "UCI_Chess960") {
        chess960 = (value == "true");
        board.set960(chess960);
    } else if (option_name == "Internal_Opening_Book") {
        internal_opening = (value == "true");
    }

    // For spsa tuning. Comment out for final build.
    // else if (option_name == "rfp_depth") {
    //     rfp_depth = std::stoi(value);
    // } else if (option_name == "rfp_c1") {
    //     rfp_c1 = std::stoi(value);
    // } 
    
    // else if (option_name == "fp_depth") {
    //     fp_depth = std::stoi(value);
    // } else if (option_name == "fp_c1") {
    //     fp_c1 = std::stoi(value);
    // } 
    
    // else if (option_name == "lmp_depth") {
    //     lmp_depth = std::stoi(value);
    // } else if (option_name == "lmp_c1") {
    //     lmp_c1 = std::stoi(value);
    // } 
    
    // else if (optionName == "hpDepth") {
    //     hpDepth = std::stoi(value);
    // } 
    
    // else if (option_name == "hp_c1") {
    //     hp_c1 = std::stoi(value);
    // } 
    
    // else if (optionName == "hpC2") {
    //     hpC2 = std::stoi(value);
    // } else if (optionName == "hpC3") {
    //     hpC3 = std::stoi(value);
    // } 

    // else if (optionName == "singularC1") {
    //     singularC1 = std::stoi(value);
    // } 
    // else if (optionName == "singularC2") {
    //     singularC2 = std::stoi(value);
    // } 
    
    // else if (option_name == "lmr_1") {
    //     lmr_1 = std::stof(value) / 100.0f;
    // } else if (option_name == "lmr_2") {
    //     lmr_2 = std::stof(value) / 100.0f;
    // } 
    
    else {
        std::cerr << "Unknown option: " << option_name << std::endl;
    }
}



// Processes the "go" command and finds the best move.
void process_go(const std::vector<std::string>& tokens) {

    // Default settings
    
    int time_limit = 30000; // Default to 15 seconds

    // Simply find the best move without considering `t` or other options
    Move best_move = Move::NO_MOVE;

    // Opening book
    if (internal_opening) {
        std::string book_move = get_book_move(board);
        if (!book_move.empty()) {
            Move move_obj = uci::uciToMove(board, book_move);
            board.makeMove(move_obj);
            std::cout << "info depth 0 score cp 0 nodes 0 time 0 pv " << book_move << std::endl;
            std::cout << "bestmove " << book_move << std::endl;
            return;
        }
    }


    // Time control:
    // Option 1: movetime <x>
    // Option 2: wtime <x> btime <x> winc <x> binc <x> movestogo <x>
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
        time_limit = movetime * adjust;
    } else {
        // Determine the time limit based on the current player's time and increment
        if (board.sideToMove() == Color::WHITE && wtime > 0) {
            int base_time = wtime / (movestogo > 0 ? movestogo + 2 : 20); 
            time_limit = static_cast<int>(base_time * adjust) + winc / 2;

            if (wtime < 20000) {
                // if only 20s left, make moves faster
                base_time = wtime / (movestogo > 0 ? movestogo + 2 : 20); 
                time_limit = static_cast<int>(base_time * adjust) + winc / 3;
            } 

            if (wtime <= 100) {
                time_limit = 50;
            } 
        } else if (board.sideToMove() == Color::BLACK && btime > 0) {
            int base_time = btime / (movestogo > 0 ? movestogo + 2 : 20); 
            time_limit = static_cast<int>(base_time * adjust) + binc / 2;

            if (btime < 20000) {
                // if only 20s left, make moves faster
                base_time = btime / (movestogo > 0 ? movestogo + 2 : 20); 
                time_limit = static_cast<int>(base_time * adjust) + binc / 3;
            }

            if (btime <= 100) {
                time_limit = 50;
            }
        }
    }
    best_move = lazysmp_root_search(board, num_threads, depth, time_limit);

    if (best_move != Move::NO_MOVE) {
        std::cout << "bestmove " << uci::moveToUci(best_move, chess960)  << std::endl;
    } else {
        std::cout << "bestmove 0000" << std::endl; // No legal moves
    }
}

void process_stop() {
    stop_search = true; 
}


// Handles the "uci" command and sends engine information
void process_uci() {
    std::cout << "id name " << ENGINE_NAME << std::endl;
    std::cout << "id author " << ENGINE_AUTHOR << std::endl;
    std::cout << "option name Threads type spin default 4 min 1 max 10" << std::endl;
    std::cout << "option name Depth type spin default 99 min 1 max 99" << std::endl;
    std::cout << "option name Hash type spin default 256 min 128 max 1024" << std::endl;
    std::cout << "option name UCI_Chess960 type check default false" << std::endl;
    std::cout << "option name Internal_Opening_Book type check default true" << std::endl;

    // For spsa tuning. Comment out for final build.
    // std::cout << "option name rfp_depth type spin default 8 min 0 max 20000" << std::endl;
    // std::cout << "option name rfp_c1 type spin default 100 min 0 max 20000" << std::endl;
    // std::cout << "option name rfpC2 type spin default 100 min 0 max 20000" << std::endl;
    // std::cout << "option name rfpC3 type spin default 100 min 0 max 20000" << std::endl;

    // std::cout << "option name fp_depth type spin default 4 min 0 max 20000" << std::endl;
    // std::cout << "option name fp_c1 type spin default 150 min 0 max 20000" << std::endl;
    // std::cout << "option name fpC2 type spin default 100 min 0 max 20000" << std::endl;
    // std::cout << "option name fpC3 type spin default 100 min 0 max 20000" << std::endl;

    // std::cout << "option name lmp_depth type spin default 4 min 0 max 20000" << std::endl;
    // std::cout << "option name lmp_c1 type spin default 6 min 0 max 20000" << std::endl;

    // std::cout << "option name hp_depth type spin default 4 min 0 max 20000" << std::endl;
    // std::cout << "option name hp_c1 type spin default 3000 min 0 max 20000" << std::endl;
    //std::cout << "option name hpC2 type spin default 3000 min 0 max 20000" << std::endl;
    //std::cout << "option name hpC3 type spin default 1000 min 0 max 20000" << std::endl;

    // std::cout << "option name singularC1 type spin default 1 min 0 max 20000" << std::endl;
    // std::cout << "option name singularC2 type spin default 25 min 0 max 20000" << std::endl;

    // std::cout << "option name lmr_1 type spin default 75 min 10 max 99" << std::endl;
    // std::cout << "option name lmr_2 type spin default 65 min 10 max 99" << std::endl;

    std::cout << "uciok" << std::endl;
}


// Main UCI loop to process commands from the GUI.
void uci_loop() {
    std::string line;
    
    while (std::getline(std::cin, line)) {
        if (line == "uci") {
            process_uci();
        } else if (line == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (line == "ucinewgame") {
            board = Board(); // Reset board to starting position
            board.set960(chess960); // Set chess960 option
        } else if (line.find("position") == 0) {
            process_position(line);
        } else if (line.find("setoption") == 0) {
            //std::cout << "set option being processed" << std::endl;
            std::vector<std::string> tokens;
            std::istringstream iss(line);
            std::string token;
            while (iss >> token) {
                tokens.push_back(token);
            }
            process_option(tokens);
        } else if (line.find("go") == 0) {
            std::vector<std::string> tokens;
            std::istringstream iss(line);
            std::string token;
            while (iss >> token) {
                tokens.push_back(token);
            }
            process_go(tokens);
        } else if (line == "stop") {
            process_stop();
        } else if (line == "quit") {
            break;
        }
    }
}

int main() {
    extract_files();
    
    std::string nnue_path = get_exec_path() + "/nnue/nnue_weights.bin";
    initialize_nnue(nnue_path);

    std::string eg_table_path = get_exec_path() + "/tables/";
    syzygy::initialize_syzygy(eg_table_path);

    uci_loop();
    return 0;
}