#include "chess.hpp"
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
#include "../lib/fathom/src/tbprobe.h"
#include "assets.hpp"
#include "parameters.hpp"
#include <filesystem>

typedef std::uint64_t U64;

using namespace chess;

int historyLMR = 7988;

int rfpScale = 25;
int rfpImproving = 11;
int rfpDepth = 10;

int singularDepth = 5;
int singularTableReduce = 4;
int singularReduceFactor = 3;

int lmpDepth = 8;
int lmpC0 = 10;
int lmpC1 = 13;
int lmpC2 = 10;
int lmpC3 = 4;  

int histC0 = 1863;
int histC1 = 1991;

int seeC1 = 118;
int seeDepth = 15;

int fpDepth = 5;
int fpC0 = 37;
int fpC1 = 113;
int fpImprovingC = 105;

int maxHistory = 18612;
int maxCaptureHistory = 6562;

int deltaC0 = 4;
int deltaC1 = 3;
int deltaC2 = 4;

float lmrC0 = 0.88f;  // 82 / 100
float lmrC1 = 0.46f;  // 39 / 100

int checkExtensions = 3;
int singularExtensions = 6;
int oneMoveExtensions = 8;

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
void extractFiles() {

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


    // Ensure "nnue" directory exists
    std::filesystem::path nnueDir = std::filesystem::path(path) / "nnue";
    if (!std::filesystem::exists(nnueDir)) {
        std::cout << "Creating directory: " << nnueDir << std::endl;
        if (!std::filesystem::create_directories(nnueDir)) {
            std::cerr << "Failed to create directory: " << nnueDir << std::endl;
            return;
        }
    }

    // Extract NNUE weights file
    std::filesystem::path nnueFilePath = nnueDir / nnueWeightFile.name;
    //if (!std::filesystem::exists(nnueFilePath)) {
        std::ofstream nnueOut(nnueFilePath, std::ios::binary);
        if (!nnueOut) {
            std::cerr << "info failed to create: " << nnueFilePath << std::endl;
        } else {
            nnueOut.write(reinterpret_cast<const char*>(nnueWeightFile.data), nnueWeightFile.size);
            nnueOut.close();
            std::cout << "info extracted: " << nnueFilePath << std::endl;
        }
    //}

}



int main() {


    // Tactical fen 
    //Board board = Board("2rq1rk1/pp3ppp/2p2n2/3p3P/3P1n2/2N2N2/PPPQ1PP1/1K1R3R b - - 2 16");
    //std::vector<std::string> pgnMoves; // Store moves in PGN format
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
    // 5rk1/p1p2pp1/4pb1p/3b4/3P2Q1/q3P3/1r1NBPPP/2RR2K1 w - - 0 22 material blunder
    // r2qr2k/6pp/2P5/bN6/2QP2n1/2P3P1/PP5P/R1B2K1R b - - 0 19 (**Qf6** vs Rh8)
    
    /* Important test cases
    5rk1/p1p2pp1/4pb1p/3b4/3P2Q1/q3P3/1r1NBPPP/2RR2K1 w - - 0 22
    r2qr2k/6pp/2P5/bN6/2QP2n1/2P3P1/PP5P/R1B2K1R b - - 0 19
    8/2p2k1p/3p4/3P3q/1p4R1/P1B2P2/4r3/Q5K1 w - - 1 42
    r2q1r1k/1b3p2/p2Ppn2/1p4Q1/8/3B4/PPP2PPP/R4RK1 w - - 1 22
    3qbrk1/5p2/8/3pP1bQ/1PpB4/2P5/6PP/5RK1 w - - 0 1
    r3r1k1/pppbq2p/3p1ppQ/2nP1P2/4P3/P6R/1BB3PP/R6K w - - 0 28
    rnbqkb1r/pp2pppp/5n2/2pp4/3P1B2/2N1P3/PPP2PPP/R2QKBNR b KQkq - 0 4
    r3nrk1/1bqpb1pp/p1n1p3/1p3p1Q/4P3/P1NRBN2/1PP1BPPP/3R2K1 b - - 3 16
    8/1pq1bpk1/p1b1pr2/3r2N1/1P5p/2P1QpP1/P1B2P1P/3RR1K1 b - - 1 30
    2r2bk1/1bq2p1p/3p2p1/p1nP4/B1P1P3/Q3RNBP/5PPK/8 w - - 2 34
    r4rk1/2qnbpp1/p1b1p3/3pP1pP/Np1N1P2/1P2B3/1PP1Q3/1K1R3R w - - 0 21
    1rb5/5pk1/p1p5/3pbPp1/5qPr/PBNQ4/1PP5/1K1R3R w - - 7 27
    */

    extractFiles();
    
    std::string nnuePath = getExecutablePath() + "/nnue/nnue_weights.bin";
    initializeNNUE(nnuePath);
    std::string path = getExecutablePath() + "/tables/";
    
    initializeTB(path);

    /*-------------------------------------------------------------------------------------------- 
        Tactical test cases
    --------------------------------------------------------------------------------------------*/
    std::vector<std::string> testFens = {
        "5rk1/p1p2pp1/4pb1p/3b4/3P2Q1/q3P3/1r1NBPPP/2RR2K1 w - - 0 22",

        "r2qr2k/6pp/2P5/bN6/2QP2n1/2P3P1/PP5P/R1B2K1R b - - 0 19",

        "8/2p2k1p/3p4/3P3q/1p4R1/P1B2P2/4r3/Q5K1 w - - 1 42",

        "r2q1r1k/1b3p2/p2Ppn2/1p4Q1/8/3B4/PPP2PPP/R4RK1 w - - 1 22",

        "3qbrk1/5p2/8/3pP1bQ/1PpB4/2P5/6PP/5RK1 w - - 0 1",

        "r3r1k1/pppbq2p/3p1ppQ/2nP1P2/4P3/P6R/1BB3PP/R6K w - - 0 28",

        "rnbqkb1r/pp2pppp/5n2/2pp4/3P1B2/2N1P3/PPP2PPP/R2QKBNR b KQkq - 0 4",

        "r3nrk1/1bqpb1pp/p1n1p3/1p3p1Q/4P3/P1NRBN2/1PP1BPPP/3R2K1 b - - 3 16",

        "8/1pq1bpk1/p1b1pr2/3r2N1/1P5p/2P1QpP1/P1B2P1P/3RR1K1 b - - 1 30",
        
        "r4rk1/2qnbpp1/p1b1p3/3pP1pP/Np1N1P2/1P2B3/1PP1Q3/1K1R3R w - - 0 21",

        "1nbqkb1r/5p1p/p2Pr1p1/1pp1p1B1/4N1n1/5N2/PP2QPPP/R3KB1R b KQk - 2 14", // fix asap

        "2br4/r1q1bpk1/2pp2pp/p3p3/P3P3/1BQRN2P/1PP2PP1/3R2K1 b - - 6 22",

        "2rbr1k1/p2n1pp1/4P2p/1p6/q1p1R3/5NB1/P1R1QPPP/5K2 b - - 0 27",
        //8/2k5/5R2/p2p1BP1/P1bP4/1p2P1K1/8/r7 b - - 0 64
        // "6k1/6pp/5p2/2b1p1n1/4P3/3P2Pq/r2NQP1P/B1R4K w - - 9 30"

        //r1bq1rk1/pp1pppbp/5np1/n3P3/3N4/1BN1B3/PPP2PPP/R2QK2R b KQ - 0 9
        //
    
    }; 

    std::vector<std::vector<Move>> testMoves = {    
        {Move::make(Square(Square::underlying::SQ_D2), Square(Square::underlying::SQ_C4))},

        {Move::make(Square(Square::underlying::SQ_D8), Square(Square::underlying::SQ_F6))},

        {Move::make(Square(Square::underlying::SQ_G4), Square(Square::underlying::SQ_G7)),
         Move::make(Square(Square::underlying::SQ_G4), Square(Square::underlying::SQ_G2))},

        {Move::make(Square(Square::underlying::SQ_G5), Square(Square::underlying::SQ_H6))},

        {Move::make(Square(Square::underlying::SQ_F1), Square(Square::underlying::SQ_F6))},

        {Move::make(Square(Square::underlying::SQ_F5), Square(Square::underlying::SQ_G6))},

        {Move::make(Square(Square::underlying::SQ_C5), Square(Square::underlying::SQ_D4)),
        Move::make(Square(Square::underlying::SQ_A7), Square(Square::underlying::SQ_A6)),
        Move::make(Square(Square::underlying::SQ_C8), Square(Square::underlying::SQ_G4)),
        },

        {Move::make(Square(Square::underlying::SQ_E8), Square(Square::underlying::SQ_F6)),
        Move::make(Square(Square::underlying::SQ_A8), Square(Square::underlying::SQ_D8))},

        {Move::make(Square(Square::underlying::SQ_H4), Square(Square::underlying::SQ_G3))},

        {Move::make(Square(Square::underlying::SQ_D4), Square(Square::underlying::SQ_C6)),
        Move::make(Square(Square::underlying::SQ_F4), Square(Square::underlying::SQ_G5))}
    };

    /*-------------------------------------------------------------------------------------------- 
        Normal test cases
    --------------------------------------------------------------------------------------------*/
    // std::vector<std::string> testFens = {
    //     "1n1r2k1/4Rppp/2b5/1p6/2p5/3pP1P1/1P1P1P1P/2B1K1R1 w - - 2 18",
    //     "r1b2rk1/5ppp/3np3/1pqpN1P1/p4P2/3B1P2/PP2Q2P/1K1R3R w - - 0 20",
    //     "r3r1k1/pbp3pp/5n2/8/3N4/2P1B3/P4P1P/R3K1R1 w Q - 4 18",
    //     "4rrk1/p2qp1bp/1pp2pp1/3p1b2/2PP1P2/1P1BP1Q1/PB4PP/2R2RK1 w - - 4 18",
    //     "3r1rk1/p4ppp/1p2p3/2q1QP2/3p4/2P3P1/PP4KP/4RR2 w - - 0 20",
    //     "1r6/3k2pp/2pb1p2/4p3/3n1P2/P1BB4/1P4PP/1K2R3 w - - 0 25",
    //     "4r3/pkp3p1/1p3p2/5P1p/2r1P3/2P3P1/PKP2R1P/3R4 w - - 0 26",
    //     "2r3k1/1p4p1/1Q3p2/1PPp1q1p/8/4P2P/6P1/R5K1 w - - 0 28",
    //     "3k4/1pp2Rp1/p3P2p/8/7P/1P6/P1P3r1/2K5 w - - 0 28",
    //     "2r5/pp4pp/3Rp1n1/5k2/8/2r3B1/PK3PPP/4R3 w - - 0 24",
    //     "2r3k1/p4ppp/8/P1p5/8/4P3/3rQPPP/1qR3K1 w - - 0 25",
    //     "r7/5ppk/2pp3p/rp4nP/4P1P1/PB1P4/1KP5/R6R w - - 4 26"
    // };

    // std::vector<std::vector<Move>> testMoves (12, {Move::NO_MOVE});

    // Default settings
    int depth = 30;
    int numThreads = 8;
    int timeLimit = 25000;

    std::cout << "Enter fen: ";
    std::string inputFen;
    std::getline(std::cin, inputFen);
    Board board = Board(inputFen);
    Move bestMove = findBestMove(board, numThreads, depth, timeLimit);

    // for (int i = 0; i < testFens.size(); i++) {
    //     std::cout << "------------------" << "Test " << i + 1 << "------------------" << std::endl;
    //     Board board = Board(testFens[i]);

    //     std::cout << board.getFen() << std::endl;

    //     Move bestMove = findBestMove(board, numThreads, depth, timeLimit, false);

    //     if (std::find(testMoves[i].begin(), testMoves[i].end(), bestMove) != testMoves[i].end()) {
    //         std::cout << "Test " << i + 1 << " passed." << std::endl;
    //     } else {
    //         std::cout << "Test " << i + 1 << " failed." << std::endl;
    //     }
    // }


    return 0;
}
