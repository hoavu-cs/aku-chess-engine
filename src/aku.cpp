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
#include "utils.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdio>  
#include <stdexcept> 
#include <thread>
#include <mutex>
#include "assets.hpp"
#include "syzygy.hpp"

using namespace chess;

// Get executable path
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

// Engine Metadata
const std::string ENGINE_NAME = "Aku Chess Engine";
const std::string ENGINE_AUTHOR = "Hoa T. Vu";

// Benchmark positions (copied from stockfish)
const std::vector<std::string> benchmark_positions = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 10",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 11",
    "4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 7 19",
    "rq3rk1/ppp2ppp/1bnpb3/3N2B1/3NP3/7P/PPPQ1PP1/2KR3R w - - 7 14 moves d4e6",
    "r1bq1r1k/1pp1n1pp/1p1p4/4p2Q/4Pp2/1BNP4/PPP2PPP/3R1RK1 w - - 2 14 moves g2g4",
    "r3r1k1/2p2ppp/p1p1bn2/8/1q2P3/2NPQN2/PPP3PP/R4RK1 b - - 2 15",
    "r1bbk1nr/pp3p1p/2n5/1N4p1/2Np1B2/8/PPP2PPP/2KR1B1R w kq - 0 13",
    "r1bq1rk1/ppp1nppp/4n3/3p3Q/3P4/1BP1B3/PP1N2PP/R4RK1 w - - 1 16",
    "4r1k1/r1q2ppp/ppp2n2/4P3/5Rb1/1N1BQ3/PPP3PP/R5K1 w - - 1 17",
    "2rqkb1r/ppp2p2/2npb1p1/1N1Nn2p/2P1PP2/8/PP2B1PP/R1BQK2R b KQ - 0 11",
    "r1bq1r1k/b1p1npp1/p2p3p/1p6/3PP3/1B2NN2/PP3PPP/R2Q1RK1 w - - 1 16",
    "3r1rk1/p5pp/bpp1pp2/8/q1PP1P2/b3P3/P2NQRPP/1R2B1K1 b - - 6 22",
    "r1q2rk1/2p1bppp/2Pp4/p6b/Q1PNp3/4B3/PP1R1PPP/2K4R w - - 2 18",
    "4k2r/1pb2ppp/1p2p3/1R1p4/3P4/2r1PN2/P4PPP/1R4K1 b - - 3 22",
    "3q2k1/pb3p1p/4pbp1/2r5/PpN2N2/1P2P2P/5PP1/Q2R2K1 b - - 4 26",
    "6k1/6p1/6Pp/ppp5/3pn2P/1P3K2/1PP2P2/3N4 b - - 0 1",
    "3b4/5kp1/1p1p1p1p/pP1PpP1P/P1P1P3/3KN3/8/8 w - - 0 1",
    "2K5/p7/7P/5pR1/8/5k2/r7/8 w - - 0 1 moves g5g6 f3e3 g6g5 e3f3",
    "8/6pk/1p6/8/PP3p1p/5P2/4KP1q/3Q4 w - - 0 1",
    "7k/3p2pp/4q3/8/4Q3/5Kp1/P6b/8 w - - 0 1",
    "8/2p5/8/2kPKp1p/2p4P/2P5/3P4/8 w - - 0 1",
    "8/1p3pp1/7p/5P1P/2k3P1/8/2K2P2/8 w - - 0 1",
    "8/pp2r1k1/2p1p3/3pP2p/1P1P1P1P/P5KR/8/8 w - - 0 1",
    "8/3p4/p1bk3p/Pp6/1Kp1PpPp/2P2P1P/2P5/5B2 b - - 0 1",
    "5k2/7R/4P2p/5K2/p1r2P1p/8/8/8 b - - 0 1",
    "6k1/6p1/P6p/r1N5/5p2/7P/1b3PP1/4R1K1 w - - 0 1",
    "1r3k2/4q3/2Pp3b/3Bp3/2Q2p2/1p1P2P1/1P2KP2/3N4 w - - 0 1",
    "6k1/4pp1p/3p2p1/P1pPb3/R7/1r2P1PP/3B1P2/6K1 w - - 0 1",
    "8/3p3B/5p2/5P2/p7/PP5b/k7/6K1 w - - 0 1",
    "5rk1/q6p/2p3bR/1pPp1rP1/1P1Pp3/P3B1Q1/1K3P2/R7 w - - 93 90",
    "4rrk1/1p1nq3/p7/2p1P1pp/3P2bp/3Q1Bn1/PPPB4/1K2R1NR w - - 40 21",
    "r3k2r/3nnpbp/q2pp1p1/p7/Pp1PPPP1/4BNN1/1P5P/R2Q1RK1 w kq - 0 16",
    "3Qb1k1/1r2ppb1/pN1n2q1/Pp1Pp1Pr/4P2p/4BP2/4B1R1/1R5K b - - 11 40",
    "4k3/3q1r2/1N2r1b1/3ppN2/2nPP3/1B1R2n1/2R1Q3/3K4 w - - 5 1",
    "5k2/8/3PK3/8/8/8/8/8 w - - 0 1",
};
std::atomic<uint64_t> benchmark_nodes{0}; // Global node counter for benchmark   

// UCI variables for search control
std::atomic<bool> search_stopped{false};
std::atomic<bool> search_running{false};
std::atomic<bool> stop_requested{false};
std::mutex search_mutex;
Move current_best_move = Move::NO_MOVE;

// Engine tunable parameters.
// int rfp_depth = 4;
// int rfp_c1 = 220; 
// int fp_depth = 3;
// int fp_c1 = 200; 
// int lmp_depth = 5;
// int lmp_c1 = 17;
// int rz_depth = 2;
// int rz_c1 = 513;
// float lmr_1 = 0.55f;
// float lmr_2 = 0.75f;
// int singular_c1 = 2;
// int singular_c2 = 0;
// int singular_bonus = 100;


int rfp_depth = 4;
int rfp_c1 = 220; 
int fp_depth = 3;
int fp_c1 = 200; 
int lmp_depth = 5;
int lmp_c1 = 17;
int rz_depth = 2;
int rz_c1 = 513;
float lmr_1 = 0.55f;
float lmr_2 = 0.55f;
int singular_c1 = 2;
int singular_c2 = 0;
int singular_bonus = 100;

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

    // Extract NNUE weights file only if it doesn't already exist
    std::string nnue_file_path = nnue_dir + "/" + nnueWeightFile.name;
    if (!std::filesystem::exists(nnue_file_path)) {
        std::ofstream nnue_out(nnue_file_path, std::ios::binary);
        if (!nnue_out) {
            std::cerr << "Failed to create: " << nnue_file_path << std::endl;
        } else {
            nnue_out.write(reinterpret_cast<const char*>(nnueWeightFile.data), nnueWeightFile.size);
            nnue_out.close();
            std::cout << "Extracted: " << nnue_file_path << std::endl;
        }
    } else {
        std::cout << "NNUE file found." << nnue_file_path << std::endl;
    }

}

// Global variables for engine options
int num_threads = 4;
int depth = 99;
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

// Processes the "position" command and sets the board state.
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

// Processes the "setoption" command to configure engine options.
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
    //      rfp_depth = std::stoi(value);
    // } else if (option_name == "rfp_c1") {
    //     rfp_c1 = std::stoi(value);
    // } else if (option_name == "fp_depth") {
    //      fp_depth = std::stoi(value);
    // } else if (option_name == "fp_c1") {
    //     fp_c1 = std::stoi(value);
    // } else if (option_name == "lmp_depth") {
    //      lmp_depth = std::stoi(value);
    // } else if (option_name == "lmp_c1") {
    //     lmp_c1 = std::stoi(value);
    // } else if (option_name == "rz_depth") {
    //      rz_depth = std::stoi(value);
    // } else if (option_name == "rz_c1") {
    //     rz_c1 = std::stoi(value);
    // } else if (option_name == "lmr_c1") {
    //     lmr_1 = std::stof(value) / 100.0f;
    // } else if (option_name == "lmr_c2") {
    //     lmr_2 = std::stof(value) / 100.0f;
    // } else if (option_name == "singular_bonus") {
    //     singular_bonus = std::stoi(value);
    // } else if (option_name == "aspiration_window") {
    //         aspiration_window = std::stoi(value);
    // } 
    
    else {
        std::cerr << "Unknown option: " << option_name << std::endl;
    }
}

// Make a thread for search. Mostly written by Jim Ablett.
void search_thread(Board search_board, int search_depth, int time_limit) {
    Move best_move = Move::NO_MOVE;
    try {
        best_move = lazysmp_root_search(search_board, num_threads, search_depth, time_limit);
    } catch (...) {
        // Handle any exceptions during search
    }
    
    // Update shared data
    {
        std::lock_guard<std::mutex> lock(search_mutex);
        current_best_move = best_move;
    }
    
    // Always output the best move found, even if search was stopped
    if (best_move != Move::NO_MOVE) {
        std::cout << "bestmove " << uci::moveToUci(best_move, chess960) << std::endl;
    } else {
        std::cout << "bestmove 0000" << std::endl; // No legal moves
    }
    
    search_running = false;
    stop_requested = false;
}

// Handles the "go" command to start the search.
void process_go(const std::vector<std::string>& tokens) {

    // Reset stop flags
    search_stopped = false;
    search_running = true;
    stop_requested = false;

    // Default settings
    int time_limit = 30000; // Default to 15 seconds
    int search_depth = depth; // Use global depth as default
    bool depth_limited = false; // Flag to indicate if depth is explicitly set

    Move best_move = Move::NO_MOVE;

    // Opening book
    if (internal_opening) {
        std::string book_move = get_book_move(board);
        if (!book_move.empty()) {
            Move move_obj = uci::uciToMove(board, book_move);
            board.makeMove(move_obj);
            std::cout << "info depth 0 score cp 0 nodes 0 time 0 pv " << book_move << std::endl;
            std::cout << "bestmove " << book_move << std::endl;
            search_running = false; 
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
        } else if (tokens[i] == "depth" && i + 1 < tokens.size()) {
            search_depth = std::stoi(tokens[i + 1]); 
            depth_limited = true;
            time_limit = INT_MAX; // Effectively unlimited time when depth is specified
        }
    }

    // Calculate time limit only if not depth-limited
    if (!depth_limited) {
        double adjust = 0.6;
        if (movetime > 0) {
            time_limit = movetime * adjust;
        } else {
            // Determine the time limit based on the current player's time and increment
            if (board.sideToMove() == Color::WHITE && wtime > 0) {
                int base_time = wtime / (movestogo > 0 ? movestogo + 2 : 20); 
                time_limit = static_cast<int>(base_time * adjust) + winc / 2;

                if (wtime < 20000) {
                    time_limit = static_cast<int>(base_time * adjust) + winc / 3;
                } 

                time_limit = std::clamp(time_limit, 0, wtime / 2 - 10); 
            } else if (board.sideToMove() == Color::BLACK && btime > 0) {
                int base_time = btime / (movestogo > 0 ? movestogo + 2 : 20); 
                time_limit = static_cast<int>(base_time * adjust) + binc / 2;

                if (btime < 20000) {
                    time_limit = static_cast<int>(base_time * adjust) + binc / 3;
                }

                time_limit = std::clamp(time_limit, 0, btime / 2 - 10);
            }
        }        
    }

    // Start search in a separate thread
    std::thread t(search_thread, board, search_depth, time_limit);
    t.detach();
}

// Processes the "stop" command to stop the search. Written by Jim Ablett.
void process_stop() {
    if (search_running && !stop_requested) {
        search_stopped = true;
        stop_requested = true;
    }
}

void process_uci() {
    std::cout << "id name " << ENGINE_NAME << std::endl;
    std::cout << "id author " << ENGINE_AUTHOR << std::endl;
    std::cout << "option name Threads type spin default 4 min 1 max 10" << std::endl;
    std::cout << "option name Depth type spin default 99 min 1 max 99" << std::endl;
    std::cout << "option name Hash type spin default 256 min 64 max 1024" << std::endl;
    std::cout << "option name UCI_Chess960 type check default false" << std::endl;
    std::cout << "option name Internal_Opening_Book type check default true" << std::endl;

    //std::cout << "option name rfp_depth type spin default 2 min 0 max 20000" << std::endl;
    //std::cout << "option name rfp_c1 type spin default 200 min 0 max 20000" << std::endl;
    //std::cout << "option name fp_depth type spin default 2 min 0 max 20000" << std::endl;
    //std::cout << "option name fp_c1 type spin default 200 min 0 max 20000" << std::endl;
    //std::cout << "option name rz_depth type spin default 2 min 0 max 20000" << std::endl;s
    //std::cout << "option name rz_c1 type spin default 550 min 0 max 20000" << std::endl;
    //std::cout << "option name lmp_depth type spin default 2 min 0 max 20000" << std::endl;
    //std::cout << "option name lmp_c1 type spin default 15 min 0 max 20000" << std::endl;
    //std::cout << "option name lmr_c1 type spin default 75 min 1 max 100" << std::endl;
    //std::cout << "option name lmr_c2 type spin default 45 min 1 max 100" << std::endl;
    //std::cout << "option name singular_bonus type spin default 100 min 0 max 20000" << std::endl;
    //std::cout << "option name aspiration_window type spin default 75 min 0 max 20000" << std::endl;

    std::cout << "uciok" << std::endl;
}

// Performs a benchmark search on a set of positions. Mostly written by Jim Ablett.
inline void benchmark(int bench_depth = 10, const std::vector<std::string>& benchmark_position = {}, bool chess960 = false) {
    // Written by Jim Ablett.
    auto start_time = std::chrono::high_resolution_clock::now();
    uint64_t total_nodes = 0;
    bool stop_search = false;
    search_stopped = false;
    search_running = false;
    stop_requested = false;
    
    std::cout << "Starting benchmark with depth " << bench_depth << std::endl;
    
    for (size_t i = 0; i < benchmark_position.size(); i++) {
        Board bench_board;
        
        try {
            std::string fen_string(benchmark_position[i]);
            
            // Handle positions with moves
            std::istringstream iss(fen_string);
            std::string base_fen;
            std::string word;
            std::vector<std::string> moves;
            bool moves_section = false;
            
            while (iss >> word) {
                if (word == "moves") {
                    moves_section = true;
                    continue;
                }
                
                if (moves_section) {
                    moves.push_back(word);
                } else {
                    if (!base_fen.empty()) base_fen += " ";
                    base_fen += word;
                }
            }
            
            bench_board = Board(base_fen);
            bench_board.set960(chess960);
            
            // Apply moves if any
            for (const auto& move_str : moves) {
                Move move = uci::uciToMove(bench_board, move_str);
                bench_board.makeMove(move);
            }
            
        } catch (const std::exception& e) {
            std::cout << "Bad FEN at position " << (i + 1) << ": " << benchmark_position[i] << std::endl;
            continue;
        }
        
        std::cout << "Position " << (i + 1) << "/" << benchmark_position.size() << ": ";
        
        // Reset node counter for this position
        benchmark_nodes.store(0);
        
        // Perform search on this position
        auto pos_start = std::chrono::high_resolution_clock::now();
        
        // Reset search state for each position
        search_stopped = false;
        search_running = true;
        stop_search = false;
        
        Move best_move = Move::NO_MOVE;
        uint64_t position_nodes = 0;
        
        try {
            // Use lazysmp_root_search with single thread for consistency
            best_move = lazysmp_root_search(bench_board, 1, bench_depth, INT_MAX);
            
            // Get the actual node count from search (now properly updated by lazysmp_root_search)
            position_nodes = benchmark_nodes.load();
            
            // Ensure we have at least some nodes counted
            if (position_nodes == 0) {
                position_nodes = 1; // Minimum to avoid division by zero
            }
            
        } catch (const std::exception& e) {
            std::cout << "Search failed: " << e.what() << std::endl;
            position_nodes = 1; // Minimum fallback
            continue;
        } catch (...) {
            std::cout << "Search failed with unknown error" << std::endl;
            position_nodes = 1; // Minimum fallback
            continue;
        }
        
        //search_running = false;
        
        auto pos_end = std::chrono::high_resolution_clock::now();
        auto pos_duration = std::chrono::duration_cast<std::chrono::milliseconds>(pos_end - pos_start).count();
        
        // Ensure minimum time to avoid division by zero
        if (pos_duration == 0) pos_duration = 1;
        
        total_nodes += position_nodes;
        std::cout << position_nodes << " nodes in " << pos_duration << "ms" << std::endl;
        
        if (search_stopped || stop_requested) {
            std::cout << "Benchmark interrupted" << std::endl;
            break;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    // Ensure minimum time to avoid division by zero
    if (total_duration == 0) total_duration = 1;
    
    uint64_t nps = total_nodes * 1000ULL / total_duration;
    
    std::cout << "==========================" << std::endl;
    std::cout << "Total time: " << total_duration << " ms" << std::endl;
    std::cout << "Nodes searched: " << total_nodes << std::endl;
    std::cout << "Nodes/second: " << nps << std::endl;
    std::cout << "==========================" << std::endl;
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
            reset_data();
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
        } else if (line.find("bench") == 0) {
            std::vector<std::string> tokens;
            std::istringstream iss(line);
            std::string token;
            while (iss >> token) {
                tokens.push_back(token);
            }
            
            int bench_depth = 10; // default depth
            if (tokens.size() > 1) {
                try {
                    bench_depth = std::stoi(tokens[1]);
                } catch (const std::exception& e) {
                    std::cout << "Invalid depth parameter, using default depth 10" << std::endl;
                }
            }
            benchmark(bench_depth, benchmark_positions, chess960);
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

    if (!initialize_nnue(nnue_path)) {
        return 1; // Exit if NNUE initialization fails
    }
    
    std::string eg_table_path = get_exec_path() + "/tables/";
    syzygy::initialize_syzygy(eg_table_path);

    uci_loop();
    return 0;
}