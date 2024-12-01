#ifndef EVAL_UTLS_HPP
#define EVAL_UTLS_HPP

#include "include/chess.hpp"

using namespace chess;

// Function declaration to check if the given color has lost castling rights
bool hasLostCastlingRights(
    const chess::Board& board, 
    chess::Color color, chess::Board::CastlingRights::Side side
);

// Function declaration for computing the value of knights on the board
int knightValue(
    const chess::Board& board, 
    int baseValue, 
    chess::Color color
);

// Function definition for computing the value of bishops on the board
int bishopValue(
    const chess::Board& board, 
    int baseValue, 
    chess::Color color
);

// Function declaration for computing the value of rooks on the board
int rookValue(
    const chess::Board& board, 
    int baseValue, 
    chess::Color color
);

// Function declaration for computing the value of queens on the board
int queenValue(
    const chess::Board& board, 
    int baseValue, 
    chess::Color color
);

// Function declaration for computing the value of pawns on the board
int pawnValue(
    const chess::Board& board, 
    int baseValue, 
    chess::Color color
);

// Function declaration for computing the value of the kings on the board
int kingValue(
    const chess::Board& board, 
    int baseValue, 
    chess::Color color
);

// Function declaration for counting the number of pieces on the board
int countPieces(
    const chess::Board& board
); 

// Function declaration for counting the legal moves
int countLegalMoves(
    const Board& board
);

// Function declaration for evaluating the board position
int evaluate(
    const chess::Board& board
);

#endif // EVAL_UTLS_HPP
