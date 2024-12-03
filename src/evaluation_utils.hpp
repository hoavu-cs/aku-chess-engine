#pragma once

#include "chess.hpp"

using namespace chess;

// Function Prototypes

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

