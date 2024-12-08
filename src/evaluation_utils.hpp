#pragma once

#include "chess.hpp"

using namespace chess;

// Function Prototypes

// Constants for the evaluation function
const int PAWN_VALUE = 100;
const int KNIGHT_VALUE = 320;
const int BISHOP_VALUE = 330;
const int ROOK_VALUE = 500;
const int QUEEN_VALUE = 900;
const int KING_VALUE = 5000;
const int CASTLE_VALUE = 100;
const int END_PIECE_COUNT = 14;
const int DOUBLE_PAWN_PENALTY = -20;
const int CENTRAL_PAWN_BONUS = 20;
const int BISHOP_PAIR_BONUS = 30;

const int ATTACK_KING_BONUS_QUEEN = 30; // Bonus for the queen attacking the enemy king, normal: 30
const int ATTACK_KING_BONUS_KNIGHT = 20; // Bonus for the knight attacking the enemy king, normal: 10
const int ATTACK_KING_BONUS_BISHOP = 20; // Bonus for the bishop attacking the enemy king, normal: 20
const int ATTACK_KING_BONUS_ROOK = 25; // Bonus for the rook attacking the enemy king, normal: 25

const int ATTACK_KING_BONUS_QUEEN_DIST = 5; // Distance for the queen to be considered attacking the enemy king, normal: 4
const int ATTACK_KING_BONUS_KNIGHT_DIST = 5; // Distance for the knight to be considered attacking the enemy king, normal: 4

const int ROOK_OPEN_FILE_BONUS = 30; // Bonus for the rook on an open file, normal: 30
const int ROOK_SEMI_OPEN_FILE_BONUS = 20; // Bonus for the rook on a semi-open file, normal: 15

const int KING_PAWN_SHIELD_BONUS = 30;
const int KING_PROTECTION_BONUS = 15;

const int PAWN_ACTIVITY_BONUS = 5;
const int KNIGHT_ACTIVITY_BONUS = 15;
const int BISHOP_ACTIVITY_BONUS = 15;
const int ROOK_ACTIVITY_BONUS = 15;
const int QUEEN_ACTIVITY_BONUS = 15;

// Knight piece-square table
const int KNIGHT_PENALTY_TABLE_WHITE[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -30,  0, 15, 15, 15, 15,  0,-30,
    -30,  5, 15, 30, 30, 15,  5,-30,
    -30,  0, 15, 30, 30, 15,  0,-30,
    -30,  5, 15, 15, 15, 15,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

const int KNIGHT_PENALTY_TABLE_BLACK[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -30,  5, 15, 15, 15, 15,  5,-30,
    -30,  0, 15, 30, 30, 15,  0,-30,
    -30,  5, 15, 30, 30, 15,  5,-30,
    -30,  0, 15, 15, 15, 15,  0,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

// Bishop piece-square table
const int BISHOP_PENALTY_TABLE_WHITE[64] = {
    -20, -20, -35, -20, -20, -35, -20, -20,
    10,   30,   0,   0,   0,   0,   30, 10,
    -10,  20,  10,  20,  20,  10,  20, -10,
    -10,   0,  30,  10,  10,  30,   0, -10,
    -10,   10,   5,  10,  10,   5,   10, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -20, -10, -10, -10, -10, -10, -10, -20
};

// Bishop piece-square table for Black
const int BISHOP_PENALTY_TABLE_BLACK[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   10,   5,  10,  10,   5,  10, -10,
    -10,   0,  30,  10,  10,  30,   0, -10,
    -10,  20,  10,  20,  20,  10,  20, -10,
    10,   30,   0,   0,   0,   0,   30,  10,
    -20, -20, -35, -20, -20, -35, -20, -20
};


/// Piece-square tables for pawns 
const int PAWN_PENALTY_TABLE_WHITE_MID[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10,-30,-30, 10, 10,  5,
     5, 5, 10,  30,  30, 10, 5,  5,
     5,  5,  5, 40, 40,  5,  5,  5,
     5,  5, 10, 50, 50, 10,  5,  5,
    20, 20, 20, 30, 30, 20, 20, 20,
    50, 50, 50, 50, 50, 50, 50, 50,
     0,  0,  0,  0,  0,  0,  0,  0
};

const int PAWN_PENALTY_TABLE_BLACK_MID[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    20, 20, 20, 30, 30, 20, 20, 20,
     5,  5, 10, 50, 50, 10,  5,  5,
     5,  5,  5, 40, 40,  5,  5,  5,
     5, 5, 10,  30,  30, 10, 5,  5,
     5, 10, 10,-30,-30, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

const int PAWN_PENALTY_TABLE_WHITE_END[64] = {
     0,    0,    0,    0,    0,    0,    0,    0,  // 1st rank
    -20,  -30,  -10,  -40,  -40,  -10,  -30,  -20, // 2nd rank 
      0,    0,   20,   50,   50,   20,    0,    0, // 3rd rank
      0,    5,   50,  100,  100,   50,    5,    0, // 4th rank 
     30,   40,   70,  150,  150,   70,   40,   30, // 5th rank
     50,   50,  100,  200,  200,  100,   50,   50, // 6th rank
     200,   300,  300,  300,  300,  300,   300,   200, // 7th rank
      0,    0,    0,    0,    0,    0,    0,    0  // 8th rank
};

const int PAWN_PENALTY_TABLE_BLACK_END[64] = {
     0,    0,    0,    0,    0,    0,    0,    0,  // 8th rank
     200,   300,  300,  300,  300,  300,   300,   200, // 7th rank
     50,   50,  100,  200,  200,  100,   50,   50, // 6th rank
     30,   40,   70,  150,  150,   70,   40,   30, // 5th rank
      0,    5,   50,  100,  100,   50,    5,    0, // 4th rank 
      0,    0,   20,   50,   50,   20,    0,    0, // 3rd rank
    -20,  -30,  -10,  -40,  -40,  -10,  -30,  -20, // 2nd rank 
      0,    0,    0,    0,    0,    0,    0,    0  // 1st rank
};

// Rook piece-square table
const int ROOK_PENALTY_TABLE_WHITE[64] = {
       0,    0,   10,   10,   10,   10,    0,    0,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
       20,   30,   30,   30,   30,   30,   30,    20,
       0,    0,    0,    0,    0,    0,    0,    0,
};

const int ROOK_PENALTY_TABLE_BLACK[64] = {
       0,    0,    0,    0,    0,    0,    0,    0,
       20,   30,   30,   30,   30,   30,   30,    20,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
      -5,    0,    0,    0,    0,    0,    0,   -5,
       0,    0,   10,    10,   10,   10,    0,    0,
};

// Queen piece-square table
const int QUEEN_PENALTY_WHITE[64] = {
     -20,  -10,  -10,   -5,   -5,  -10,  -10,  -20,
     -10,    0,    0,    0,    0,    5,    0,  -10,
     -10,    0,    5,    5,    5,    5,    5,  -10,
      -5,    0,    5,    5,    5,    5,    0,    0,
      -5,    0,    5,    5,    5,    5,    0,   -5,
     -10,    0,    5,    5,    5,    5,    0,  -10,
     -10,    0,    0,    0,    0,    0,    0,  -10,
     -20,  -10,  -10,   -5,   -5,  -10,  -10,  -20,
};

const int QUEEN_PENALTY_BLACK[64] = {
     -20,  -10,  -10,   -5,   -5,  -10,  -10,  -20,
     -10,    0,    0,    0,    0,    0,    0,  -10,
     -10,    0,    5,    5,    5,    5,    0,  -10,
      -5,    0,    5,    5,    5,    5,    0,   -5,
      -5,    0,    5,    5,    5,    5,    0,    0,
     -10,    0,    5,    5,    5,    5,    5,  -10,
     -10,    0,    0,    0,    0,    5,    0,  -10,
     -20,  -10,  -10,   -5,   -5,  -10,  -10,  -20,
};

// King piece-square table
const int KING_PENALTY_TABLE_WHITE_MID[64] = {
      20,   75,  75,    0,    0,   10,  75,   20,
      20,   20,  -25,  -25,  -25,  -25,   20,   20,
     -10,  -20,  -20,  -20,  -20,  -20,  -20,  -10,
     -20,  -30,  -30,  -40,  -40,  -30,  -30,  -20,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
};

const int KING_PENALTY_TABLE_BLACK_MID[64] = {
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -30,  -40,  -40,  -50,  -50,  -40,  -40,  -30,
     -20,  -30,  -30,  -40,  -40,  -30,  -30,  -20,
     -10,  -20,  -20,  -20,  -20,  -20,  -20,  -10,
      20,   20,  -25,  -25,  -25,   -25,   20,   20,
      20,   75,  75,    0,    0,   10,  75,   20,
};

const int KING_PENALTY_TABLE_WHITE_END[64] = {
     -50,  -30,  -30,  -30,  -30,  -30,  -30,  -50,
     -30,  -30,    0,    0,    0,    0,  -30,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -20,  -10,    0,    0,  -10,  -20,  -30,
     -50,  -40,  -30,  -20,  -20,  -30,  -40,  -50,
};

const int KING_PENALTY_TABLE_BLACK_END[64] = {
     -50,  -40,  -30,  -20,  -20,  -30,  -40,  -50,
     -30,  -20,  -10,    0,    0,  -10,  -20,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   30,   40,   40,   30,  -10,  -30,
     -30,  -10,   20,   30,   30,   20,  -10,  -30,
     -30,  -30,    0,    0,    0,    0,  -30,  -30,
     -50,  -30,  -30,  -30,  -30,  -30,  -30,  -50,
};

/**
 * Check if the given color has lost castling rights on the specified side.
 * @param board The chess board.
 * @param color The color to check.
 * @param side The castling side (king-side or queen-side).
 * @return True if the castling rights are lost, otherwise false.
 */
bool hasLostCastlingRights(const chess::Board& board, chess::Color color, chess::Board::CastlingRights::Side side);

/**
 * Compute the value of knights on the board.
 * @param board The chess board.
 * @param baseValue The base value of a knight.
 * @param color The color of knights to evaluate.
 * @return The total value of knights for the specified color.
 */
int knightValue(const chess::Board& board, int baseValue, chess::Color color);

/**
 * Compute the value of bishops on the board.
 * @param board The chess board.
 * @param baseValue The base value of a bishop.
 * @param color The color of bishops to evaluate.
 * @return The total value of bishops for the specified color.
 */
int bishopValue(const chess::Board& board, int baseValue, chess::Color color);

/**
 * Compute the value of pawns on the board.
 * @param board The chess board.
 * @param baseValue The base value of a pawn.
 * @param color The color of pawns to evaluate.
 * @return The total value of pawns for the specified color.
 */
int pawnValue(const chess::Board& board, int baseValue, chess::Color color);

/**
 * Compute the value of rooks on the board.
 * @param board The chess board.
 * @param baseValue The base value of a rook.
 * @param color The color of rooks to evaluate.
 * @return The total value of rooks for the specified color.
 */
int rookValue(const chess::Board& board, int baseValue, chess::Color color);

/**
 * Compute the value of queens on the board.
 * @param board The chess board.
 * @param baseValue The base value of a queen.
 * @param color The color of queens to evaluate.
 * @return The total value of queens for the specified color.
 */
int queenValue(const chess::Board& board, int baseValue, chess::Color color);

/**
 * Compute the value of kings on the board.
 * @param board The chess board.
 * @param baseValue The base value of a king.
 * @param color The color of kings to evaluate.
 * @return The total value of kings for the specified color.
 */
int kingValue(const chess::Board& board, int baseValue, chess::Color color);

/**
 * Count the total number of pieces on the board.
 * @param board The chess board.
 * @return The total number of pieces on the board.
 */
int countPieces(const chess::Board& board);

/**
 * Count the total number of legal moves for the current side.
 * @param board The chess board.
 * @return The total number of legal moves for the current side.
 */
int countLegalMoves(const chess::Board& board);

/**
 * Evaluate the board position for the current side to move.
 * @param board The chess board.
 * @return The evaluation score of the position (positive if white is better, negative if black is better).
 */
int evaluate(const chess::Board& board);

/**
 * Check if the specified file is open (no pawns on it).
 * @param board The chess board.
 * @param file The file to check.
 * @return True if the file is open, otherwise false.
 */
bool isOpenFile(const chess::Board& board, const File& file);

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
bool isSemiOpenFile(const chess::Board& board, const File& file, chess::Color color);

/**
 * Generate a bitboard mask for the specified file.
 * @param file The file for which to generate the mask.
 * @return The bitboard mask for the specified file.
 */
Bitboard generateFileMask(const File& file);


/**
 * Returns the activity score for the given color.
 */
int activity(const chess::Board& board, const chess::Color color);
