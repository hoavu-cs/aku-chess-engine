#pragma once

#include "chess.hpp"

using namespace chess;

// Function Prototypes

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
      4,  15,  16,   0,   7,  21,  33,   1,
      0,  15,  15,  15,  14,  27,  18,  10,
     -6,  13,  13,  26,  34,  12,  10,   4,
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
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
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
    -26,  -4,  -4, -10,   3,   3,  33, -12,
    -27,  -2,  -5,  12,  17,   6,  10, -25,
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
    -27,  -2,  -5,  12,  17,   6,  10, -25,
    -26,  -4,  -4, -10,   3,   3,  33, -12,
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
    -15,  36,  12, -54,   8, -28,  24,  14,
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
    -15,  36,  12, -54,   8, -28,  24,  14,
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

/**
 * Generate a bitboard mask for the specified file.
 * @param file The file for which to generate the mask.
 * @return The bitboard mask for the specified file.
 */
Bitboard generateFileMask(const File& file);

/**
 * Returns whether the game is in an endgame state.
 */
bool isEndGame(const Board& board);

/**
* @brief Check if the given square is a passed pawn.
* @param squareIndex The index of the square to check.
* @param color The color of the pawn.
* @param enemyPawns The bitboard of enemy pawns.
* @return True if the pawn is passed, otherwise false.
*/
bool isPassedPawn(int squareIndex, Color color, const Bitboard& enemyPawns);

/**
 * Compute the Manhattan distance between two squares.
 * @param sq1 The first square.
 * @param sq2 The second square.
 * @return The Manhattan distance between the two squares.
 */
int manhattanDistance(const Square& sq1, const Square& sq2);

/*------------------------------------------------------------------------
    Main Functions
------------------------------------------------------------------------*/

/**
 * Compute the value of pawns on the board.
 * @param board The chess board.
 * @param baseValue The base value of a pawn.
 * @param color The color of pawns to evaluate.
 * @param endGameFlag A flag indicating whether the game is in the endgame.
 * @return The total value of pawns for the specified color.
 */
int pawnValue(const Board& board, int baseValue, Color color, bool endGameFlag);

/**
 * Compute the value of knights on the board.
 * @param board The chess board.
 * @param baseValue The base value of a knight.
 * @param color The color of knights to evaluate.
 * @param endGameFlag A flag indicating whether the game is in the endgame.
 * @return The total value of knights for the specified color.
 */
int knightValue(const Board& board, int baseValue, Color color, bool endGameFlag);

/**
 * Compute the value of bishops on the board.
 * @param board The chess board.
 * @param baseValue The base value of a bishop.
 * @param color The color of bishops to evaluate.
 * @param endGameFlag A flag indicating whether the game is in the endgame.
 * @return The total value of bishops for the specified color.
 */
int bishopValue(const Board& board, int baseValue, Color color, bool endGameFlag);

/**
 * Compute the value of rooks on the board.
 * @param board The chess board.
 * @param baseValue The base value of a rook.
 * @param color The color of rooks to evaluate.
 * @param endGameFlag A flag indicating whether the game is in the endgame.
 * @return The total value of rooks for the specified color.
 */
int rookValue(const Board& board, int baseValue, Color color, bool endGameFlag);

/**
 * Compute the value of queens on the board.
 * @param board The chess board.
 * @param baseValue The base value of a queen.
 * @param color The color of queens to evaluate.
 * @param endGameFlag A flag indicating whether the game is in the endgame.
 * @return The total value of queens for the specified color.
 */
int queenValue(const Board& board, int baseValue, Color color, bool endGameFlag);

/**
 * Compute the value of kings on the board.
 * @param board The chess board.
 * @param baseValue The base value of a king.
 * @param color The color of kings to evaluate.
 * @param endGameFlag A flag indicating whether the game is in the endgame.
 * @return The total value of kings for the specified color.
 */
int kingValue(const Board& board, int baseValue, Color color, bool endGameFlag);

/**
 * Evaluate the board position for the current side to move.
 * @param board The chess board.
 * @return The evaluation score of the position (positive if white is better, negative if black is better).
 */
int evaluate(const Board& board);



