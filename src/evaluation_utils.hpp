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

// Knight piece-square table for White
const int whiteKnightTable[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -30, 10, 15, 15, 15, 15, 10,-30,
    -30,  0, 15, 30, 30, 15,  0,-30,
    -30,  0, 15, 30, 30, 15,  0,-30,
    -30,  0, 15, 15, 15, 15,  0,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

// Knight piece-square table for Black
const int blackKnightTable[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -30,  5, 15, 15, 15, 15,  5,-30,
    -30,  0, 15, 30, 30, 15,  0,-30,
    -30,  0, 15, 30, 30, 15,  0,-30,
    -30, 10, 15, 15, 15, 15, 10,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

// Bishop piece-square table for White
const int whiteBishopTable[64] = {
    -20, -20, -35, -20, -20, -35, -20, -20,
     10,  20,   0,   0,   0,   0,  20,  10,
    -10,  20,  10,  20,  20,  10,  20, -10,
    -10,   0,  30,  10,  10,  30,   0, -10,
    -10,  10,   5,  10,  10,   5,  10, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -20, -10, -10, -10, -10, -10, -10, -20
};

// Bishop piece-square table for Black
const int blackBishopTable[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,  10,   5,  10,  10,   5,  10, -10,
    -10,   0,  30,  10,  10,  30,   0, -10,
    -10,  20,  10,  20,  20,  10,  20, -10,
     10,  20,   0,   0,   0,   0,  20,  10,
    -20, -20, -35, -20, -20, -35, -20, -20
};

// Pawn piece-square tables for White in the middle game
const int whitePawnTableMid[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10,-30,-30, 10, 10,  10,
     5, 5, 10,  30,  30, 10, 5,  5,
     5,  5,  5, 40, 40,  5,  5,  5,
     5,  5,  5, 40, 40,  5,  5,  5,
     20, 20, 20, 30, 30, 20, 20, 20,
    20, 20, 20, 30, 30, 20, 20, 20,
     0,  0,  0,  0,  0,  0,  0,  0
};

// Pawn piece-square tables for Black in the middle game
const int blackPawnTableMid[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    20, 20, 20, 30, 30, 20, 20, 20,
    20, 20, 20, 30, 30, 20, 20, 20,
     5,  5,  5, 40, 40,  5,  5,  5,
     5,  5,  5, 40, 40,  5,  5,  5,
     5, 5, 10,  30,  30, 10, 5,  5,
     5, 10, 10,-30,-30, 10, 10, 10,
     0,  0,  0,  0,  0,  0,  0,  0
};

// Pawn piece-square tables for White in the end game
const int whitePawnTableEnd[64] = {
     0,    0,    0,    0,    0,    0,    0,    0,  // 1st rank
    -10,  -15,   -5,  -20,  -20,   -5,  -15,  -10, // 2nd rank 
      0,    0,   10,   25,   25,   10,    0,    0, // 3rd rank
      0,    2,   25,   50,   50,   25,    2,    0, // 4th rank 
     15,   20,   35,   75,   75,   35,   20,   15, // 5th rank
     25,   25,   50,  100,  100,   50,   25,   25, // 6th rank
     100,  150,  150,  150,  150,  150,   150,  100, // 7th rank
      0,    0,    0,    0,    0,    0,    0,    0  // 8th rank
};

// Pawn piece-square tables for Black in the end game
const int blackPawnTableEnd[64] = {
     0,    0,    0,    0,    0,    0,    0,    0,  // 8th rank
     100,  150,  150,  150,  150,  150,   150,  100, // 7th rank
     25,   25,   50,  100,  100,   50,   25,   25, // 6th rank
     15,   20,   35,   75,   75,   35,   20,   15, // 5th rank
      0,    2,   25,   50,   50,   25,    2,    0, // 4th rank 
      0,    0,   10,   25,   25,   10,    0,    0, // 3rd rank
    -10,  -15,   -5,  -20,  -20,   -5,  -15,  -10, // 2nd rank 
      0,    0,    0,    0,    0,    0,    0,    0  // 1st rank
};


// Rook piece-square table for White
const int whiteRookTable[64] = {
       0,    0,    0,   10,   10,    0,    0,    0,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
       20,   30,   30,   30,   30,   30,   30,    20,
       0,    0,    0,    0,    0,    0,    0,    0,
};

// Rook piece-square table for Black
const int blackRookTable[64] = {
       0,    0,    0,    0,    0,    0,    0,    0,
       20,   30,   30,   30,   30,   30,   30,    20,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
       0,    0,    0,    10,   10,   0,    0,    0,
};

// Queen piece-square table for White
const int whiteQueenTable[64] = {
     -20,  -10,  -10,   -5,   -5,  -10,  -10,  -20,
     -10,    0,    0,    0,    0,    5,    0,  -10,
     -10,    0,    5,    5,    5,    5,    5,  -10,
      -5,    0,    5,    5,    5,    5,    0,    0,
      -5,    0,    5,    5,    5,    5,    0,   -5,
     -10,    0,    5,    5,    5,    5,    0,  -10,
     -10,    0,    0,    0,    0,    0,    0,  -10,
     -20,  -10,  -10,   -5,   -5,  -10,  -10,  -20,
};

// Queen piece-square table for Black
const int blackQueenTable[64] = {
     -20,  -10,  -10,   -5,   -5,  -10,  -10,  -20,
     -10,    0,    0,    0,    0,    0,    0,  -10,
     -10,    0,    5,    5,    5,    5,    0,  -10,
      -5,    0,    5,    5,    5,    5,    0,   -5,
      -5,    0,    5,    5,    5,    5,    0,    0,
     -10,    0,    5,    5,    5,    5,    5,  -10,
     -10,    0,    0,    0,    0,    5,    0,  -10,
     -20,  -10,  -10,   -5,   -5,  -10,  -10,  -20,
};

// King piece-square table for White in the middle game
const int whiteKingTableMid[64] = {
      20,   50,   50,    0,    0,   10,   50,   20,
      20,   20,  -25,  -25,  -25,  -25,   20,   20,
     -10,  -20,  -20,  -20,  -20,  -20,  -20,  -10,
     -20,  -30,  -30,  -40,  -40,  -30,  -30,  -20,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
};

// King piece-square table for Black in the middle game
const int blackKingTableMid[64] = {
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -20,  -30,  -30,  -40,  -40,  -30,  -30,  -20,
     -10,  -20,  -20,  -20,  -20,  -20,  -20,  -10,
      20,   20,  -25,  -25,  -25,   -25,   20,   20,
      20,   50,  50,    0,    0,   10,  50,   20,
};

// King piece-square table for White in the end game
const int whiteKingTableEnd[64] = {
     -50,  -30,  -30,  -30,  -30,  -30,  -30,  -50,
     -30,  -30,    0,    0,    0,    0,  -30,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -20,  -10,    0,    0,  -10,  -20,  -30,
     -50,  -40,  -30,  -20,  -20,  -30,  -40,  -50,
};

// King piece-square table for Black in the middle game
const int blackKingTableEnd[64] = {
     -50,  -40,  -30,  -20,  -20,  -30,  -40,  -50,
     -30,  -20,  -10,    0,    0,  -10,  -20,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -30,    0,    0,    0,    0,  -30,  -30,
     -50,  -30,  -30,  -30,  -30,  -30,  -30,  -50,
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

/**
 * Check if the given color has lost castling rights on the specified side.
 * @param board The chess board.
 * @param color The color to check.
 * @param side The castling side (king-side or queen-side).
 * @return True if the castling rights are lost, otherwise false.
 */
bool hasLostCastlingRights(const Board& board, Color color, Board::CastlingRights::Side side);

/**
 * Compute the value of knights on the board.
 * @param board The chess board.
 * @param baseValue The base value of a knight.
 * @param color The color of knights to evaluate.
 * @return The total value of knights for the specified color.
 */
int knightValue(const Board& board, int baseValue, Color color);

/**
 * Compute the value of bishops on the board.
 * @param board The chess board.
 * @param baseValue The base value of a bishop.
 * @param color The color of bishops to evaluate.
 * @return The total value of bishops for the specified color.
 */
int bishopValue(const Board& board, int baseValue, Color color);

/**
 * Compute the value of pawns on the board.
 * @param board The chess board.
 * @param baseValue The base value of a pawn.
 * @param color The color of pawns to evaluate.
 * @return The total value of pawns for the specified color.
 */
int pawnValue(const Board& board, int baseValue, Color color);

/**
 * Compute the value of rooks on the board.
 * @param board The chess board.
 * @param baseValue The base value of a rook.
 * @param color The color of rooks to evaluate.
 * @return The total value of rooks for the specified color.
 */
int rookValue(const Board& board, int baseValue, Color color);

/**
 * Compute the value of queens on the board.
 * @param board The chess board.
 * @param baseValue The base value of a queen.
 * @param color The color of queens to evaluate.
 * @return The total value of queens for the specified color.
 */
int queenValue(const Board& board, int baseValue, Color color);

/**
 * Compute the value of kings on the board.
 * @param board The chess board.
 * @param baseValue The base value of a king.
 * @param color The color of kings to evaluate.
 * @return The total value of kings for the specified color.
 */
int kingValue(const Board& board, int baseValue, Color color);

/**
 * Count the total number of pieces on the board.
 * @param board The chess board.
 * @return The total number of pieces on the board.
 */
int countPieces(const Board& board);

/**
 * Evaluate the board position for the current side to move.
 * @param board The chess board.
 * @return The evaluation score of the position (positive if white is better, negative if black is better).
 */
int evaluate(const Board& board);

/**
 * Check if the specified file is open (no pawns on it).
 * @param board The chess board.
 * @param file The file to check.
 * @return True if the file is open, otherwise false.
 */
bool isOpenFile(const Board& board, const File& file);

/**
 * Compute the Manhattan distance between two squares.
 * @param sq1 The first square.
 * @param sq2 The second square.
 * @return The Manhattan distance between the two squares.
 */
int manhattanDistance(const Square& sq1, const Square& sq2);

/**
 * Check if the specified file is semi-open (no pawns of the given color).
 * @param board The chess board.
 * @param file The file to check.
 * @param color The color to check against.
 * @return True if the file is semi-open, otherwise false.
 */
bool isSemiOpenFile(const Board& board, const File& file, Color color);

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
* @brief Generates a bitboard mask for all squares in the given file.
* @param file The file for which to generate the mask (A through H).
* @return A Bitboard with bits set for all squares in the specified file.
 */
Bitboard generateFileMask(const File& file);

/**
* @brief Check if the given square is a passed pawn.
* @param squareIndex The index of the square to check.
* @param color The color of the pawn.
* @param enemyPawns The bitboard of enemy pawns.
* @return True if the pawn is passed, otherwise false.
*/
bool isPassedPawn(int squareIndex, Color color, const Bitboard& enemyPawns);

/**
 * @brief Check if the specified file is open (no pawns on it).
 * @param board The chess board.
 * @param file The file to check.
 * @return True if the file is open, otherwise false.
 */
bool isOpenFile(const Board& board, const File& file);


/** 
 * @brief Check if the specified file is semi-open (no pawns of the given color).
 * @param board The chess board.
 * @param file The file to check.
 * @param color The color to check against.
 * @return True if the file is semi-open, otherwise false.
 */
bool isSemiOpenFile(const Board& board, const File& file, Color color);