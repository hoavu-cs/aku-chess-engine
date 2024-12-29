#pragma once

#include "chess.hpp"

using namespace chess;


struct Info {
    std::vector<bool> openFiles;         // 8 elements, each for a file's openness (true or false)
    std::vector<bool> semiOpenFilesWhite; // 8 elements for white's semi-open files
    std::vector<bool> semiOpenFilesBlack; // 8 elements for black's semi-open files
    bool endGameFlag = false;
    
    // Constructor to initialize the vectors with 8 elements, all set to false
    Info() 
        : openFiles(8, false), 
          semiOpenFilesWhite(8, false), 
          semiOpenFilesBlack(8, false) 
    {}
};



/*------------------------------------------------------------------------
    Helper Functions
------------------------------------------------------------------------*/

/**
 * Generate a bitboard mask for the specified file.
 * @param file The file for which to generate the mask.
 * @return The bitboard mask for the specified file.
 */
Bitboard generateFileMask(int file);

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
bool isPassedPawn(int sqIndex, Color color, const Bitboard& enemyPawns);

/**
 * Compute the Manhattan distance between two squares.
 * @param sq1 The first square.
 * @param sq2 The second square.
 * @return The Manhattan distance between the two squares.
 */
int manhattanDistance(const Square& sq1, const Square& sq2);

/* 
 * Check if the given square is an outpost for the given color.
 * @param board The chess board.
 * @param sqIndex The index of the square to check.
 * @param color The color of the outpost.
 * @return True if the square is an outpost, otherwise false.
*/
bool isOutpost(const Board& board, int sqIndex, Color color);

/* 
 *   Open file check
*/
bool isOpenFile(const chess::Board& board, int file);

/* 
 *   Semi-open file check from the perspective of the given color
*/
bool isSemiOpenFile(const chess::Board& board, int file, Color color); 

/*------------------------------------------------------------------------
    Main Functions
------------------------------------------------------------------------*/

/**
 * Compute the value of pawns on the board.
 * @param board The chess board.
 * @param baseValue The base value of a pawn.
 * @param color The color of pawns to evaluate.
 * @return The total value of pawns for the specified color.
 */
int pawnValue(const Board& board, int baseValue, Color color, Info& info);

/**
 * Compute the value of knights on the board.
 * @param board The chess board.
 * @param baseValue The base value of a knight.
 * @param color The color of knights to evaluate.
 * @return The total value of knights for the specified color.
 */
int knightValue(const Board& board, int baseValue, Color color, Info& info);

/**
 * Compute the value of bishops on the board.
 * @param board The chess board.
 * @param baseValue The base value of a bishop.
 * @param color The color of bishops to evaluate.
 * @return The total value of bishops for the specified color.
 */
int bishopValue(const Board& board, int baseValue, Color color, Info& info);

/**
 * Compute the value of rooks on the board.
 * @param board The chess board.
 * @param baseValue The base value of a rook.
 * @param color The color of rooks to evaluate.
 * @return The total value of rooks for the specified color.
 */
int rookValue(const Board& board, int baseValue, Color color, Info& info);

/**
 * Compute the value of queens on the board.
 * @param board The chess board.
 * @param baseValue The base value of a queen.
 * @param color The color of queens to evaluate.
 * @return The total value of queens for the specified color.
 */
int queenValue(const Board& board, int baseValue, Color color, Info& info);

/**
 * Compute the value of kings on the board.
 * @param board The chess board.
 * @param baseValue The base value of a king.
 * @param color The color of kings to evaluate.
 * @return The total value of kings for the specified color.
 */
int kingValue(const Board& board, int baseValue, Color color, Info& info);

/**
 * Compute the reward for center control.
 */
int centerControl(const Board& board, Color color);

/**
 * Evaluate the board position for the current side to move.
 * @param board The chess board.
 * @return The evaluation score of the position (positive if white is better, negative if black is better).
 */
int evaluate(const Board& board);



