#include "chess.hpp"
#include "evaluation.hpp"
#include <tuple>
#include <unordered_map>
#include <cstdint>
#include <map>
#include <omp.h> 

using namespace chess; 

/*--------------------------------------------------------------------------
    Tables, Constants, and Global Variables
------------------------------------------------------------------------*/

// Constants for the evaluation function
const int PAWN_VALUE = 100;
const int KNIGHT_VALUE = 320;
const int BISHOP_VALUE = 330;
const int ROOK_VALUE = 510;
const int QUEEN_VALUE = 900;
const int KING_VALUE = 5000;

// Knight piece-square tables
const int whiteKnightTableMid[64] = {
    -105, -21, -58, -33, -17, -28, -19,  -23,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -13,   4,  16,  13,  28,  19,  21,   -8,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -47,  60,  37,  65,  84, 129,  73,   44,
     -73, -41,  72,  36,  23,  62,   7,  -17,
    -167, -89, -34, -49,  61, -97, -15, -107,
};

const int blackKnightTableMid[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23,
};

const int whiteKnightTableEnd[64] = {
     -29, -51, -23, -15, -22, -18, -50,  -64,
     -42, -20, -10,  -5,  -2, -20, -23,  -44,
     -23,  -3,  -1,  15,  10,  -3, -20,  -22,
     -18,  -6,  16,  25,  16,  17,   4,  -18,
     -17,   3,  22,  22,  22,  11,   8,  -18,
     -24, -20,  10,   9,  -1,  -9, -19,  -41,
     -25,  -8, -25,  -2,  -9, -25, -24,  -52,
     -58, -38, -13, -28, -31, -27, -63,  -99,
};

const int blackKnightTableEnd[64] = {
     -58, -38, -13, -28, -31, -27, -63,  -99,
     -25,  -8, -25,  -2,  -9, -25, -24,  -52,
     -24, -20,  10,   9,  -1,  -9, -19,  -41,
     -17,   3,  22,  22,  22,  11,   8,  -18,
     -18,  -6,  16,  25,  16,  17,   4,  -18,
     -23,  -3,  -1,  15,  10,  -3, -20,  -22,
     -42, -20, -10,  -5,  -2, -20, -23,  -44,
     -29, -51, -23, -15, -22, -18, -50,  -64,
};

// Bishop piece-square tablesint 
const int whiteBishopTableMid[64] = {
    -33,  -3, -14, -21, -13, -12, -39, -21,
      4,  25,  16,   0,   7,  21,  33,   1,
      0,  15,  15,  15,  14,  27,  18,  10,
     -6,  13,  20,  26,  34,  20,  10,   4,
     -4,   5,  19,  50,  37,  37,   7,  -2,
    -16,  37,  43,  40,  35,  50,  37,  -2,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -29,   4, -82, -37, -25, -42,   7,  -8,
};

const int blackBishopTableMid[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  20,  26,  34,  20,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  25,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};

const int whiteBishopTableEnd[64] = {
    -23,  -9, -23,  -5,  -9, -16,  -5, -17,
    -14, -18,  -7,  -1,   4,  -9, -15, -27,
    -12,  -3,   8,  10,  13,   3,  -7, -15,
     -6,   3,  13,  19,   7,  10,  -3,  -9,
     -3,   9,  12,   9,  14,  10,   3,   2,
      2,  -8,   0,  -1,  -2,   6,   0,   4,
     -8,  -4,   7, -12,  -3, -13,  -4, -14,
    -14, -21, -11,  -8,  -7,  -9, -17, -24,
};

const int blackBishopTableEnd[64] = {
    -14, -21, -11,  -8, -7,  -9, -17, -24,
     -8,  -4,   7, -12, -3, -13,  -4, -14,
      2,  -8,   0,  -1, -2,   6,   0,   4,
     -3,   9,  12,   9, 14,  10,   3,   2,
     -6,   3,  13,  19,  7,  10,  -3,  -9,
    -12,  -3,   8,  10, 13,   3,  -7, -15,
    -14, -18,  -7,  -1,  4,  -9, -15, -27,
    -23,  -9, -23,  -5, -9, -16,  -5, -17,
};

// Pawn piece-square tables for White in the middle game
const int whitePawnTableMid[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    -35,  -1, -20, -35, -35,  24,  38, -22,
    -26,  -4,  -4,  10,  10,   3,  33, -12,
    -27,  -2,  15,  35,  35,   6,  10, -25,
    -14,  13,   6,  21,  23,  12,  17, -23,
     -6,   7,  26,  31,  65,  56,  25, -20,
     98, 134,  61,  95,  68, 126,  34, -11,
      0,   0,   0,   0,   0,   0,   0,   0,
};

// Pawn piece-square tables for Black in the middle game
const int blackPawnTableMid[64] = {
     0,   0,   0,   0,   0,   0,   0,   0,
     98, 134,  61,  95,  68, 126,  34, -11,
     -6,   7,  26,  31,  65,  56,  25, -20,
    -14,  13,   6,  21,  23,  12,  17, -23,
    -27,  -2,  15,  35,  35,   6,  10, -25,
    -26,  -4,  -4,  10,  10,   3,  33, -12,
    -35,  -1, -20, -35, -35,  24,  38, -22,
      0,   0,   0,   0,   0,   0,   0,   0,
};

// Pawn piece-square tables for White in the end game
const int whitePawnTableEnd[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     13,   8,   8,  10,  13,   0,   2,  -7,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
     32,  24,  13,   5,  -2,   4,  17,  17,
     94, 100,  85,  67,  56,  53,  82,  84,
    178, 173, 158, 134, 147, 132, 165, 187,
      0,   0,   0,   0,   0,   0,   0,   0,
};

// Pawn piece-square tables for Black in the end game
const int blackPawnTableEnd[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};


// Rook piece-square tables
const int whiteRookTableMid[64] = {
    -19, -13,   1,  17,  16,   7, -37, -26,
    -44, -16, -20,  -9,  -1,  11,  -6, -71,
    -45, -25, -16, -17,   3,   0,  -5, -33,
    -36, -26, -12,  -1,   9,  -7,   6, -23,
    -24, -11,   7,  26,  24,  35,  -8, -20,
     -5,  19,  26,  36,  17,  45,  61,  16,
     27,  32,  58,  62,  80,  67,  26,  44,
     32,  42,  32,  51,  63,   9,  31,  43,
};

const int blackRookTableMid[64] = {
     32,  42,  32,  51, 63,  9,  31,  43,
     27,  32,  58,  62, 80, 67,  26,  44,
     -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26,
};

const int whiteRookTableEnd[64] = {
     -9,   2,   3,  -1,  -5, -13,   4, -20,
     -6,  -6,   0,   2,  -9,  -9, -11,  -3,
     -4,   0,  -5,  -1,  -7, -12,  -8, -16,
      3,   5,   8,   4,  -5,  -6,  -8, -11,
      4,   3,  13,   1,   2,   1,  -1,   2,
      7,   7,   7,   5,   4,  -3,  -5,  -3,
     11,  13,  13,  11,  -3,   3,   8,   3,
     13,  10,  18,  15,  12,  12,   8,   5,
};

const int blackRookTableEnd[64] = {
    13, 10, 18, 15, 12,  12,   8,   5,
    11, 13, 13, 11, -3,   3,   8,   3,
     7,  7,  7,  5,  4,  -3,  -5,  -3,
     4,  3, 13,  1,  2,   1,  -1,   2,
     3,  5,  8,  4, -5,  -6,  -8, -11,
    -4,  0, -5, -1, -7, -12,  -8, -16,
    -6, -6,  0,  2, -9,  -9, -11,  -3,
    -9,  2,  3, -1, -5, -13,   4, -20,
};

// Queen piece-square tables
const int whiteQueenTableMid[64] = {
     -1, -18,  -9,  10, -15, -25, -31, -50,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -28,   0,  29,  12,  59,  44,  43,  45,
};

const int blackQueenTableMid[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};

const int whiteQueenTableEnd[64] = {
    -33, -28, -22, -43,  -5, -32, -20, -41,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -18,  28,  19,  47,  31,  34,  39,  23,
      3,  22,  24,  45,  57,  40,  57,  36,
    -20,   6,   9,  49,  47,  35,  19,   9,
    -17,  20,  32,  41,  58,  25,  30,   0,
     -9,  22,  22,  27,  27,  19,  10,  20,
};


const int blackQueenTableEnd[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};



// King piece-square tables
const int whiteKingTableMid[64] = {
    -15,  3,  35, -54,   8, -28,  35,  14,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -14, -14, -22, -46, -44, -30, -15, -27,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -17, -20, -12, -27, -30, -25, -14, -36,
     -9,  24,   2, -16, -20,   6,  22, -22,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
    -65,  23,  16, -15, -56, -34,   2,  13,
};

const int blackKingTableMid[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  40,  35, -54,   8, -28,  35,  14,
};

const int whiteKingTableEnd[64] = {
    -53, -34, -21, -11, -28, -14, -24, -43,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -18,  -4,  21,  24,  27,  23,   9, -11,
     -8,  22,  24,  27,  26,  33,  26,   3,
     10,  17,  23,  15,  20,  45,  44,  13,
    -12,  17,  14,  17,  17,  38,  23,  11,
    -74, -35, -18, -18, -11,  15,   4, -17,
};

const int blackKingTableEnd[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43
};

const std::unordered_map<int, std::vector<int>> adjSquares = {
    {0, {1, 8, 9}}, 
    {1, {0, 2, 8, 9, 10}}, 
    {2, {1, 3, 9, 10, 11}}, 
    {3, {2, 4, 10, 11, 12}}, 
    {4, {3, 5, 11, 12, 13}}, 
    {5, {4, 6, 12, 13, 14}}, 
    {6, {5, 7, 13, 14, 15}}, 
    {7, {6, 14, 15}}, 
    {8, {0, 1, 9, 16, 17}}, 
    {9, {0, 1, 2, 8, 10, 16, 17, 18}}, 
    {10, {1, 2, 3, 9, 11, 17, 18, 19}}, 
    {11, {2, 3, 4, 10, 12, 18, 19, 20}}, 
    {12, {3, 4, 5, 11, 13, 19, 20, 21}}, 
    {13, {4, 5, 6, 12, 14, 20, 21, 22}}, 
    {14, {5, 6, 7, 13, 15, 21, 22, 23}}, 
    {15, {6, 7, 14, 22, 23}}, 
    {16, {8, 9, 17, 24, 25}}, 
    {17, {8, 9, 10, 16, 18, 24, 25, 26}}, 
    {18, {9, 10, 11, 17, 19, 25, 26, 27}}, 
    {19, {10, 11, 12, 18, 20, 26, 27, 28}}, 
    {20, {11, 12, 13, 19, 21, 27, 28, 29}}, 
    {21, {12, 13, 14, 20, 22, 28, 29, 30}}, 
    {22, {13, 14, 15, 21, 23, 29, 30, 31}}, 
    {23, {14, 15, 22, 30, 31}}, 
    {24, {16, 17, 25, 32, 33}}, 
    {25, {16, 17, 18, 24, 26, 32, 33, 34}}, 
    {26, {17, 18, 19, 25, 27, 33, 34, 35}}, 
    {27, {18, 19, 20, 26, 28, 34, 35, 36}}, 
    {28, {19, 20, 21, 27, 29, 35, 36, 37}}, 
    {29, {20, 21, 22, 28, 30, 36, 37, 38}}, 
    {30, {21, 22, 23, 29, 31, 37, 38, 39}}, 
    {31, {22, 23, 30, 38, 39}}, 
    {32, {24, 25, 33, 40, 41}}, 
    {33, {24, 25, 26, 32, 34, 40, 41, 42}}, 
    {34, {25, 26, 27, 33, 35, 41, 42, 43}}, 
    {35, {26, 27, 28, 34, 36, 42, 43, 44}}, 
    {36, {27, 28, 29, 35, 37, 43, 44, 45}}, 
    {37, {28, 29, 30, 36, 38, 44, 45, 46}}, 
    {38, {29, 30, 31, 37, 39, 45, 46, 47}}, 
    {39, {30, 31, 38, 46, 47}}, 
    {40, {32, 33, 41, 48, 49}}, 
    {41, {32, 33, 34, 40, 42, 48, 49, 50}}, 
    {42, {33, 34, 35, 41, 43, 49, 50, 51}}, 
    {43, {34, 35, 36, 42, 44, 50, 51, 52}}, 
    {44, {35, 36, 37, 43, 45, 51, 52, 53}}, 
    {45, {36, 37, 38, 44, 46, 52, 53, 54}}, 
    {46, {37, 38, 39, 45, 47, 53, 54, 55}}, 
    {47, {38, 39, 46, 54, 55}}, 
    {48, {40, 41, 49, 56, 57}}, 
    {49, {40, 41, 42, 48, 50, 56, 57, 58}}, 
    {50, {41, 42, 43, 49, 51, 57, 58, 59}}, 
    {51, {42, 43, 44, 50, 52, 58, 59, 60}}, 
    {52, {43, 44, 45, 51, 53, 59, 60, 61}}, 
    {53, {44, 45, 46, 52, 54, 60, 61, 62}}, 
    {54, {45, 46, 47, 53, 55, 61, 62, 63}}, 
    {55, {46, 47, 54, 62, 63}}, 
    {56, {48, 49, 57}}, 
    {57, {48, 49, 50, 56, 58}}, 
    {58, {49, 50, 51, 57, 59}}, 
    {59, {50, 51, 52, 58, 60}}, 
    {60, {51, 52, 53, 59, 61}}, 
    {61, {52, 53, 54, 60, 62}}, 
    {62, {53, 54, 55, 61, 63}}, 
    {63, {54, 55, 62}}
};


/*------------------------------------------------------------------------
    Helper Functions
------------------------------------------------------------------------*/

// Return true if the game is in the endgame phase 
bool isEndGame(const Board& board) {
    const int materialThreshold = 32;
    const int knightValue = 3, bishopValue = 3, rookValue = 5, queenValue = 9;

    int totalValue = board.pieces(PieceType::KNIGHT, Color::WHITE).count() * knightValue
                  + board.pieces(PieceType::BISHOP, Color::WHITE).count() * bishopValue
                  + board.pieces(PieceType::ROOK, Color::WHITE).count() * rookValue
                  + board.pieces(PieceType::QUEEN, Color::WHITE).count() * queenValue
                  + board.pieces(PieceType::KNIGHT, Color::BLACK).count() * knightValue
                  + board.pieces(PieceType::BISHOP, Color::BLACK).count() * bishopValue
                  + board.pieces(PieceType::ROOK, Color::BLACK).count() * rookValue
                  + board.pieces(PieceType::QUEEN, Color::BLACK).count() * queenValue;

    if (totalValue <= materialThreshold) {
        return true;
    } else {
        return false;
    }
}

/*------------------------------------------------------------------------
    Utility Functions
------------------------------------------------------------------------*/

// Function to visualize a bitboard
void bitBoardVisualize(const Bitboard& board) {
    std::uint64_t boardInt = board.getBits();

    for (int i = 0; i < 64; i++) {
        if (i % 8 == 0) {
            std::cout << std::endl;
        }
        if (boardInt & (1ULL << i)) {
            std::cout << "1 ";
        } else {
            std::cout << "0 ";
        }
    }
    std::cout << std::endl;
}

// Generate a bitboard mask for the specified file
Bitboard generateFileMask(int file) {
    constexpr Bitboard fileMasks[] = {
        0x0101010101010101ULL, // File A
        0x0202020202020202ULL, // File B
        0x0404040404040404ULL, // File C
        0x0808080808080808ULL, // File D
        0x1010101010101010ULL, // File E
        0x2020202020202020ULL, // File F
        0x4040404040404040ULL, // File G
        0x8080808080808080ULL  // File H
    };

    // Return the corresponding file mask
    if (file >= 0 && file < 8) {
        return Bitboard(fileMasks[file]);
    }

    // Return an empty bitboard for invalid files
    return Bitboard(0ULL);
}

// Check if the given square is a passed pawn
bool isPassedPawn(int sqIndex, Color color, const Bitboard& theirPawns) {
    int file = sqIndex % 8;
    int rank = sqIndex / 8;

    Bitboard opponentPawns = theirPawns;
    while (opponentPawns) {
        int sqIndex2 = opponentPawns.lsb();  
        int file2 = sqIndex2 % 8;
        int rank2 = sqIndex2 / 8;

        if (std::abs(file - file2) <= 1 && rank2 > rank && color == Color::WHITE) {
            return false; 
        }

        if (std::abs(file - file2) <= 1 && rank2 < rank && color == Color::BLACK) {
            return false; 
        }

        opponentPawns.clear(sqIndex2);
    }

    return true;  
}

// Function to compute the Manhattan distance between two squares
int manhattanDistance(const Square& sq1, const Square& sq2) {
    return std::abs(sq1.file() - sq2.file()) + std::abs(sq1.rank() - sq2.rank());
}

// Check if a square is an outpost
bool isOutpost(const Board& board, int sqIndex, Color color) {
    int file = sqIndex % 8, rank = sqIndex / 8;

    // Outposts must be in the opponent's half of the board
    if ((color == Color::WHITE && rank < 4) || (color == Color::BLACK && rank > 3)) {
        return false;
    }

    // Get our pawns and their pawns
    Bitboard ourPawns = board.pieces(PieceType::PAWN, color);
    Bitboard theirPawns = board.pieces(PieceType::PAWN, !color);

    // Check for support from our pawns
    int frontRank = (color == Color::WHITE) ? rank - 1 : rank + 1;
    Bitboard supportMask = (file > 0 ? (1ULL << (frontRank * 8 + file - 1)) : 0) |
                           (file < 7 ? (1ULL << (frontRank * 8 + file + 1)) : 0);
    if (!(ourPawns & supportMask)) {
        return false; // Not supported by our pawns
    }

    // Check for potential attacks from opponent pawns (including future pushes)
    for (int r = rank + 1; r < 8 && color == Color::WHITE; ++r) {
        if (file > 0 && (theirPawns & (1ULL << (r * 8 + file - 1)))) {
            return false; // Controlled by opponent pawn on the left
        }
        if (file < 7 && (theirPawns & (1ULL << (r * 8 + file + 1)))) {
            return false; // Controlled by opponent pawn on the right
        }
    }
    for (int r = rank - 1; r >= 0 && color == Color::BLACK; --r) {
        if (file > 0 && (theirPawns & (1ULL << (r * 8 + file - 1)))) {
            return false; // Controlled by opponent pawn on the left
        }
        if (file < 7 && (theirPawns & (1ULL << (r * 8 + file + 1)))) {
            return false; // Controlled by opponent pawn on the right
        }
    }

    return true;
}


bool isOpenFile(const chess::Board& board, int file) {
    // Get bitboards for white and black pawns
    Bitboard whitePawns = board.pieces(PieceType::PAWN, Color::WHITE);
    Bitboard blackPawns = board.pieces(PieceType::PAWN, Color::BLACK);

    // Generate the mask for the given file
    Bitboard mask = generateFileMask(File(file));

    // A file is open if it has no pawns of either color
    return !(whitePawns & mask) && !(blackPawns & mask);
}

bool isSemiOpenFile(const chess::Board& board, int file, Color color) {
    // Get the bitboard for the pawns of the given color
    Bitboard ownPawns = board.pieces(PieceType::PAWN, color);

    // Generate the mask for the given file
    Bitboard mask = generateFileMask(File(file));

    // A file is semi-open if it has no pawns of the given color
    return !(ownPawns & mask);
}

bool isProtectedByPawn(int sqIndex, const Board& board, Color color) {
    // Get the file and rank of the square
    int file = sqIndex % 8;
    int rank = sqIndex / 8;

    // White pawns protect diagonally from behind
    if (color == Color::WHITE) {
        if (file > 0 && board.at(Square((rank - 1) * 8 + (file - 1))) == PieceType::PAWN) {
            return true; // Protected from bottom-left
        }
        if (file < 7 && board.at(Square((rank - 1) * 8 + (file + 1))) == PieceType::PAWN) {
            return true; // Protected from bottom-right
        }
    }
    // Black pawns protect diagonally from behind
    else {
        if (file > 0 && board.at(Square((rank + 1) * 8 + (file - 1))) == PieceType::PAWN) {
            return true; // Protected from top-left
        }
        if (file < 7 && board.at(Square((rank + 1) * 8 + (file + 1))) == PieceType::PAWN) {
            return true; // Protected from top-right
        }
    }

    return false; // No protecting pawn found
}

/*------------------------------------------------------------------------
 Main Functions 
------------------------------------------------------------------------*/

// Compute the value of the pawns on the board
int pawnValue(const Board& board, int baseValue, Color color, Info& info) {

    Bitboard ourPawns = board.pieces(PieceType::PAWN, color);
    Bitboard theirPawns = board.pieces(PieceType::PAWN, !color);

    std::uint64_t ourPawnsBits = ourPawns.getBits();
    std::uint64_t theirPawnsBits = theirPawns.getBits();
    bool endGameFlag = info.endGameFlag;


    // constants
    const int passedPawnBonus = 25;
    const int protectedPassedPawnBonus = 35;
    const int isolatedPawnPenalty = 20;
    const int unSupportedPenalty = 10;
    const int doubledPawnPenalty = 25;
    const int* pawnTable;

    if (color == Color::WHITE) {
        if (endGameFlag) {
            pawnTable = whitePawnTableEnd;
        } else {
            pawnTable = whitePawnTableMid;
        }
    } else {
        if (endGameFlag) {
            pawnTable = blackPawnTableEnd;
        } else {
            pawnTable = blackPawnTableMid;
        }
    }

    int files[8] = {0};
    int value = 0;
    int advancedPawnBonus = endGameFlag ? 5 : 2;

    Bitboard theirPieces = board.pieces(PieceType::BISHOP, !color) 
                            | board.pieces(PieceType::KNIGHT, !color) 
                            | board.pieces(PieceType::ROOK, !color) 
                            | board.pieces(PieceType::QUEEN, !color); 
                            
    Bitboard ourPawnsCopy = ourPawns;
    while (!ourPawns.empty()) {
        int sqIndex = ourPawns.lsb(); 
        int file = sqIndex % 8; // Get the file of the pawn
        files[file]++; // Increment the count of pawns on the file
        ourPawns.clear(sqIndex);
    }
    ourPawns = ourPawnsCopy;

    while (!ourPawns.empty()) {
        int sqIndex = ourPawns.lsb(); 

        value += baseValue; 
        value += pawnTable[sqIndex]; 

        int file = sqIndex % 8;
        int rank = sqIndex / 8;

        if ((file == 0 && files[1] == 0) || 
            (file == 7 && files[6] == 0) || 
            (file > 0 && file < 7 && files[file - 1] == 0 && files[file + 1] == 0)) {
            value -= isolatedPawnPenalty;
        }

        if (isPassedPawn(sqIndex, color, theirPawns)) {
            if (isProtectedByPawn(sqIndex, board, color)) {
                value += protectedPassedPawnBonus;
            } else {
                value += passedPawnBonus;
            }
        }  else {
            if (!isProtectedByPawn(sqIndex, board, color)) {
                // If a pawn is unsupported, penalize it
                // Penalize more if the pawn is on a semi-open file 
                
                if (color == Color::WHITE && info.semiOpenFilesBlack[file]) {
                    value -= unSupportedPenalty;
                } else if (color == Color::BLACK && info.semiOpenFilesWhite[file]) {
                    value -= unSupportedPenalty;
                } else  {
                    value -= (unSupportedPenalty - 5); // Penalize less if the pawn is not on a semi-open file
                }
            }
        }      


        if (color == Color::WHITE) {
            value += (rank - 1) * advancedPawnBonus;
        } else {
            value += (6 - rank) * advancedPawnBonus;
        }

        ourPawns.clear(sqIndex);
    }
    
    // Add penalty for doubled pawns
    for (int i = 0; i < 8; i++) {
        if (files[i] > 1) {
            value -= (files[i] - 1) * doubledPawnPenalty;
        }
    }

    return value;
}

// Compute the value of the knights on the board
int knightValue(const Board& board, int baseValue, Color color, Info& info) {

    // Constants
    const int* knightTable;
    const int outpostBonus = 30;
    
    bool endGameFlag = info.endGameFlag;
 
    if (color == Color::WHITE) {
        if (endGameFlag) {
            knightTable = whiteKnightTableEnd;
        } else {
            knightTable = whiteKnightTableMid;
        }
    } else {
        if (endGameFlag) {
            knightTable = blackKnightTableEnd;
        } else {
            knightTable = blackKnightTableMid;
        }
    } 

    Bitboard knights = board.pieces(PieceType::KNIGHT, color);
    int value = 0;

    while (!knights.empty()) {
        value += baseValue;
        int sqIndex = knights.lsb();
        value += knightTable[sqIndex];

        if (isOutpost(board, sqIndex, color)) {
            value += outpostBonus;
        }

        knights.clear(sqIndex);
    }

    return value;
}

// Compute the value of the bishops on the board
int bishopValue(const Board& board, int baseValue, Color color, Info& info) {

    // Constants
    const int bishopPairBonus = 30;
    const int mobilityBonus = 2;
    const int outpostBonus = 30;
    const int *bishopTable;

    bool endGameFlag = info.endGameFlag;

    if (color == Color::WHITE) {
        if (endGameFlag) {
            bishopTable = whiteBishopTableEnd;
        } else {
            bishopTable = whiteBishopTableMid;
        }
    } else {
        if (endGameFlag) {
            bishopTable = blackBishopTableEnd;
        } else {
            bishopTable = blackBishopTableMid;
        }
    }

    Bitboard bishops = board.pieces(PieceType::BISHOP, color);
    int value = 0;
    
    if (bishops.count() >= 2) {
        value += bishopPairBonus;
    }
 
    while (!bishops.empty()) {
        value += baseValue;
        int sqIndex = bishops.lsb();
        value += bishopTable[sqIndex];
        Bitboard bishopMoves = attacks::bishop(Square(sqIndex), board.occ());
        value += bishopMoves.count() * mobilityBonus;

        if (isOutpost(board, sqIndex, color)) {
                        value += outpostBonus;
        }

        bishops.clear(sqIndex);
    }

    return value;
}


// Compute the total value of the rooks on the board
int rookValue(const Board& board, int baseValue, Color color, Info& info) {

    // Constants
    const double mobilityBonus = 1;
    const int* rookTable;
    const int semiOpenFileBonus = 15;
    const int openFileBonus = 20;

    bool endGameFlag = info.endGameFlag;

    if (color == Color::WHITE) {
        if (endGameFlag) {
            rookTable = whiteRookTableEnd;
        } else {
            rookTable = whiteRookTableMid;
        }
    } else {
        if (endGameFlag) {
            rookTable = blackRookTableEnd;
        } else {
            rookTable = blackRookTableMid;
        }
    }

    Bitboard rooks = board.pieces(PieceType::ROOK, color);
    int value = 0;

    while (!rooks.empty()) {
        int sqIndex = rooks.lsb(); 
        int file = sqIndex % 8; 

        value += baseValue; // Add the base value of the rook
        value += rookTable[sqIndex]; // Add the value from the piece-square table

        if (info.openFiles[file]) {
            value += openFileBonus;
        } else {
            if (info.semiOpenFilesWhite[file] && color == Color::WHITE) {
                value += semiOpenFileBonus;
            } else if (info.semiOpenFilesBlack[file] && color == Color::BLACK) {
                value += semiOpenFileBonus;
            }
        }
        
        Bitboard rookMoves = attacks::rook(Square(sqIndex), board.occ());
        value += mobilityBonus * rookMoves.count();

        rooks.clear(sqIndex); // Remove the processed rook
    }
    return value;
}

// Compute the total value of the queens on the board
int queenValue(const Board& board, int baseValue, Color color, Info& info) {

    // Constants
    const int* queenTable;
    //const int mobilityBonus = 1;
    //const int attackedPenalty = 10;

    bool endGameFlag = info.endGameFlag;

    if (color == Color::WHITE) {
        if (endGameFlag) {
            queenTable = whiteQueenTableEnd;
        } else {
            queenTable = whiteQueenTableMid;
        }
    } else {
        if (endGameFlag) {
            queenTable = blackQueenTableEnd;
        } else {
            queenTable = blackQueenTableMid;
        }
    }
    
    Bitboard queens = board.pieces(PieceType::QUEEN, color);
    Bitboard theirKing = board.pieces(PieceType::KING, !color);

    Square theirKingSQ = Square(theirKing.lsb()); // Get the square of the their king
    int value = 0;

    while (!queens.empty()) {
        int sqIndex = queens.lsb(); 
        int queenRank = sqIndex / 8, queenFile = sqIndex % 8;
        value += baseValue; 
        value += queenTable[sqIndex]; 

        // Bitboard queenMoves = attacks::queen(Square(sqIndex), board.occ());
        // value += mobilityBonus * queenMoves.count();

        queens.clear(sqIndex); 
    }
    return value;
}

// Compute the value of the kings on the board
int kingValue(const Board& board, int baseValue, Color color, Info& info) {

    // Constants
    const int pawnShieldBonus = 30;
    const int pieceProtectionBonus = 30;
    const int* kingTable;
    const int openFileThreat = 40;

    bool endGameFlag = info.endGameFlag;

    if (color == Color::WHITE) {
        if (endGameFlag) {
            kingTable = whiteKingTableEnd;
        } else {
            kingTable = whiteKingTableMid;
        }
    } else {
        if (endGameFlag) {
            kingTable = blackKingTableEnd;
        } else {
            kingTable = blackKingTableMid;
        }
    }

    Bitboard king = board.pieces(PieceType::KING, color);
    const PieceType allPieceTypes[] = {PieceType::KNIGHT, PieceType::BISHOP, PieceType::ROOK, PieceType::QUEEN};
    
    int value = baseValue;
    int sqIndex = king.lsb();
    int kingRank = sqIndex / 8, kingFile = sqIndex % 8;
    

    value += kingTable[sqIndex];    

    if (!endGameFlag) {
        // King protection by pawns
        Bitboard ourPawns = board.pieces(PieceType::PAWN, color);
        while (!ourPawns.empty()) {
            int pawnIndex = ourPawns.lsb();
            int pawnRank = pawnIndex / 8, pawnFile = pawnIndex % 8;
            // if the pawn is in front of the king and on an adjacent file, add shield bonus
            if (color == Color::WHITE 
                        && pawnRank == kingRank + 1 && std::abs(pawnFile - kingFile) <= 1) {
                value += pawnShieldBonus;
            } else if (color == Color::BLACK 
                        && pawnRank == kingRank - 1 && std::abs(pawnFile - kingFile) <= 1) {
                value += pawnShieldBonus; 
            }

            ourPawns.clear(pawnIndex);
        }
        
        // King protection by pieces
        for (const auto& type : allPieceTypes) {
            Bitboard pieces = board.pieces(type, color);

            while (!pieces.empty()) {
                int pieceSqIndex = pieces.lsb();
                if (color == Color::WHITE) {
                    if (pieceSqIndex / 8 > sqIndex / 8 && manhattanDistance(Square(pieceSqIndex), Square(sqIndex)) <= 4) { 
                        value += pieceProtectionBonus; // if the piece is in front of the king with distance <= 2
                    }
                } else if (color == Color::BLACK) {
                    if (pieceSqIndex / 8 < sqIndex / 8 && manhattanDistance(Square(pieceSqIndex), Square(sqIndex)) <= 4) {
                        value += pieceProtectionBonus; // if the piece is in front of the king with distance <= 2
                    }
                }

                pieces.clear(pieceSqIndex);
            }
        }

        // Penalty for being attacked
        Bitboard attackers;
        int numAttackers;

        Bitboard adjSq;
        for (const auto& adjSqIndex : adjSquares.at(sqIndex)) {
            adjSq = adjSq | Bitboard::fromSquare(adjSqIndex);
        }
        
        // // Get the bitboard of attackers
        // for (const auto& sq : adjSquares.at(sqIndex)) {
        //     attackers = attackers | attacks::attackers(board, !color, Square(sq));
        // }

        Bitboard theirPawns = board.pieces(PieceType::PAWN, !color);
        while (theirPawns) {
            int pawnIndex = theirPawns.lsb();
            if (manhattanDistance(Square(pawnIndex), Square(sqIndex)) <= 3) {
                attackers.set(pawnIndex);
            }
            theirPawns.clear(pawnIndex);
        }

        theirPawns = board.pieces(PieceType::PAWN, !color);
        Bitboard theirQueens = board.pieces(PieceType::QUEEN, !color);
        while (theirQueens) {
            int queenIndex = theirQueens.lsb();
            Bitboard queenAttacks = attacks::queen(Square(queenIndex), theirPawns);
            if (queenAttacks & adjSq) {
                attackers.set(queenIndex);
            }
            theirQueens.clear(queenIndex);
        }

        Bitboard theirRooks = board.pieces(PieceType::ROOK, !color);
        while (theirRooks) {
            int rookIndex = theirRooks.lsb();
            Bitboard rookAttacks = attacks::rook(Square(rookIndex), theirPawns);
            if (rookAttacks & adjSq) {
                attackers.set(rookIndex);
            }
            theirRooks.clear(rookIndex);
        }

        Bitboard theirKnight = board.pieces(PieceType::KNIGHT, !color);
        while (theirKnight) {
            int knightIndex = theirKnight.lsb();
            Bitboard knightAttacks = attacks::knight(Square(knightIndex));
            if (knightAttacks & adjSq) {
                attackers.set(knightIndex);
            }
            theirKnight.clear(knightIndex);
        }

        Bitboard theirBishop = board.pieces(PieceType::BISHOP, !color);
        while (theirBishop) {
            int bishopIndex = theirBishop.lsb();
            Bitboard bishopAttacks = attacks::bishop(Square(bishopIndex), theirPawns);

            if (bishopAttacks & adjSq) {
                attackers.set(bishopIndex);
            }
            theirBishop.clear(bishopIndex);
        }

        numAttackers = attackers.count();
        double attackWeight = 0;
        double threatScore = 0;

        // The more attackers, the higher the penalty
        switch (numAttackers) {
            case 0: attackWeight = 0; break;
            case 1: attackWeight = 0.25; break;
            case 2: attackWeight = 0.50; break;
            case 3: attackWeight = 0.75; break;
            case 4: attackWeight = 1.00; break;
            case 5: attackWeight = 1.25; break;
            case 6: attackWeight = 1.50; break;
            case 7: attackWeight = 1.75; break;
            case 8: attackWeight = 2.00; break;
            default: break;
        }


        while (attackers) {
            int attackerIndex = attackers.lsb();
            Piece attacker = board.at(Square(attackerIndex));

            if (attacker.type() == PieceType::PAWN) {
                threatScore += attackWeight * 10;
            } else if (attacker.type() == PieceType::KNIGHT) {
                threatScore += attackWeight * 30;
            } else if (attacker.type() == PieceType::BISHOP) {
                threatScore += attackWeight * 30;
            } else if (attacker.type() == PieceType::ROOK) {
                threatScore += attackWeight * 50;
            } else if (attacker.type() == PieceType::QUEEN) {
                threatScore += attackWeight * 100;
            }

            attackers.clear(attackerIndex);
        }
        
        value -= static_cast<int>(threatScore);
    }

    return value;
}

// Compute the reward for controlling the center
int centerControl(const Board& board, Color color) {
    int e4 = 28, d4 = 27, e5 = 36, d5 = 35;
    return 0;
}

// Function to evaluate the board position
int evaluate(const Board& board) {
    // Initialize totals
    int whiteScore = 0;
    int blackScore = 0;

    // Mop-up phase: if only their king is left
    Info info;
    info.endGameFlag = isEndGame(board);

    /*--------------------------------------------------------------------------
        Mop-up phase: if only their king is left without any other pieces.
        Aim to checkmate.
    --------------------------------------------------------------------------*/

    Color theirColor = !board.sideToMove();
    Bitboard theirPieces = board.pieces(PieceType::PAWN, theirColor) | board.pieces(PieceType::KNIGHT, theirColor) | 
                           board.pieces(PieceType::BISHOP, theirColor) | board.pieces(PieceType::ROOK, theirColor) | 
                           board.pieces(PieceType::QUEEN, theirColor);

    Bitboard ourPieces =  board.pieces(PieceType::ROOK, board.sideToMove()) | board.pieces(PieceType::QUEEN, board.sideToMove());

    if (theirPieces.count() == 0 && ourPieces.count() > 0) {
        Square ourKing = Square(board.pieces(PieceType::KING, board.sideToMove()).lsb());
        Square theirKing = Square(board.pieces(PieceType::KING, theirColor).lsb());
        Square E4 = Square(28);

        int kingDist = manhattanDistance(ourKing, theirKing);
        int distToCenter = manhattanDistance(theirKing, E4);
        int ourMaterial = 900 * board.pieces(PieceType::QUEEN, board.sideToMove()).count() + 
                          500 * board.pieces(PieceType::ROOK, board.sideToMove()).count() + 
                          300 * board.pieces(PieceType::BISHOP, board.sideToMove()).count() + 
                          300 * board.pieces(PieceType::KNIGHT, board.sideToMove()).count() + 
                          100 * board.pieces(PieceType::PAWN, board.sideToMove()).count(); // avoid throwing away pieces
        int score = 5000 +  distToCenter + (14 - kingDist);

        return board.sideToMove() == Color::WHITE ? score : -score;
    }

    /*--------------------------------------------------------------------------
        Standard evaluation phase
    --------------------------------------------------------------------------*/

    // Compute open files and semi-open files
    for (int i = 0; i < 8; i++) {
        info.openFiles[i] = isOpenFile(board, i);
        if (!info.openFiles[i]) {
            info.semiOpenFilesWhite[i] = isSemiOpenFile(board, i, Color::WHITE);
            info.semiOpenFilesBlack[i] = isSemiOpenFile(board, i, Color::BLACK);
        }
    }

    // Traverse each piece type
    const PieceType allPieceTypes[] = {PieceType::PAWN, PieceType::KNIGHT, PieceType::BISHOP, 
                                           PieceType::ROOK, PieceType::QUEEN, PieceType::KING};

    for (const auto& type : allPieceTypes) {
        // Determine base value of the current piece type
        int baseValue = 0;

        switch (type.internal()) {
            case PieceType::PAWN: baseValue = PAWN_VALUE; break;
            case PieceType::KNIGHT: baseValue = KNIGHT_VALUE; break;
            case PieceType::BISHOP: baseValue = BISHOP_VALUE; break;
            case PieceType::ROOK: baseValue = ROOK_VALUE; break;
            case PieceType::QUEEN: baseValue = QUEEN_VALUE; break;
            case PieceType::KING: baseValue = KING_VALUE; break;
            default: break;
        }

        // Process white pieces
        if (type == PieceType::KNIGHT) {
            whiteScore += knightValue(board, baseValue, Color::WHITE, info);
            blackScore += knightValue(board, baseValue, Color::BLACK, info);
        } else if (type == PieceType::BISHOP) {
            whiteScore += bishopValue(board, baseValue, Color::WHITE, info);
            blackScore += bishopValue(board, baseValue, Color::BLACK, info);
        } else if (type == PieceType::KING) {
            whiteScore += kingValue(board, baseValue, Color::WHITE, info);
            blackScore += kingValue(board, baseValue, Color::BLACK, info);
        }  else if (type == PieceType::PAWN) {
            whiteScore += pawnValue(board, baseValue, Color::WHITE, info);
            blackScore += pawnValue(board, baseValue, Color::BLACK, info);
        } else if (type == PieceType::ROOK) {
            whiteScore += rookValue(board, baseValue, Color::WHITE, info);
            blackScore += rookValue(board, baseValue, Color::BLACK, info);
        } else if (type == PieceType::QUEEN) {
            whiteScore += queenValue(board, baseValue, Color::WHITE, info);
            blackScore += queenValue(board, baseValue, Color::BLACK, info);
        } 
    }

    // Avoid trading pieces for pawns in middle game
    const int knightValue = 3, bishopValue = 3, rookValue = 5, queenValue = 9;
    int whitePieceValue = queenValue * board.pieces(PieceType::QUEEN, Color::WHITE).count() + 
                        rookValue * board.pieces(PieceType::ROOK, Color::WHITE).count() + 
                        bishopValue * board.pieces(PieceType::BISHOP, Color::WHITE).count() + 
                        knightValue * board.pieces(PieceType::KNIGHT, Color::WHITE).count();

    int blackPieceValue = queenValue * board.pieces(PieceType::QUEEN, Color::BLACK).count() + 
                        rookValue * board.pieces(PieceType::ROOK, Color::BLACK).count() + 
                        bishopValue * board.pieces(PieceType::BISHOP, Color::BLACK).count() + 
                        knightValue * board.pieces(PieceType::KNIGHT, Color::BLACK).count();

    if (whitePieceValue > blackPieceValue) {
        whiteScore += 50;
    } else if (blackPieceValue > whitePieceValue) {
        blackScore += 50;
    }

    return whiteScore - blackScore;
}
