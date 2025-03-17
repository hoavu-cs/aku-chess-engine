/*
* Author: Hoa T. Vu
* Created: December 1, 2024
* 
* Copyright (c) 2024 Hoa T. Vu
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to use,
* copy, modify, merge, publish, and distribute copies of the Software for 
* **non-commercial purposes only**, provided that the following conditions are met:
* 
* 1. The above copyright notice and this permission notice shall be included in
*    all copies or substantial portions of the Software.
* 2. Any use of this Software for commercial purposes **requires prior written
*    permission from the author, Hoa T. Vu**.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/


#include "chess.hpp"

using namespace chess; 

const int PAWN_VALUE = 120;
const int KNIGHT_VALUE = 320; 
const int BISHOP_VALUE = 330;
const int ROOK_VALUE = 500;
const int QUEEN_VALUE = 900;
const int KING_VALUE = 5000;

const int mate[64] = {
    600, 500, 500, 400, 400, 500, 500, 600,
    500, 500, 150, 150, 150, 150, 200, 500,
    500, 150, 50,  50,  50,  50,  150, 500,
    400, 150, 50,  0,   0,   50,  150, 400,
    400, 150, 50,  0,   0,   50,  150, 400,
    500, 150, 50,  50,  50,  50,  150, 500,
    500, 200, 150, 150, 150, 150, 200, 500,
    600, 500, 500, 400, 400, 500, 500, 600
};

int bnMateLightSquares[64] = {
    0, 10, 20, 30, 40, 50, 60, 70,
    10, 20, 30, 40, 50, 60, 70, 60,
    20, 30, 40, 50, 60, 70, 60, 50,
    30, 40, 50, 60, 70, 60, 50, 40,
    40, 50, 60, 70, 60, 50, 40, 30,
    50, 60, 70, 60, 50, 40, 30, 20,
    60, 70, 60, 50, 40, 30, 20, 10,
    70, 60, 50, 40, 30, 20, 10, 0
};


int bnMateDarkSquares[64] = {
    70, 60, 50, 40, 30, 20, 10, 0,
    60, 70, 60, 50, 40, 30, 20, 10,
    50, 60, 70, 60, 50, 40, 30, 20,
    40, 50, 60, 70, 60, 50, 40, 30,
    30, 40, 50, 60, 70, 60, 50, 40,
    20, 30, 40, 50, 60, 70, 60, 50,
    10, 20, 30, 40, 50, 60, 70, 60,
    0, 10, 20, 30, 40, 50, 60, 70
};




int midPawnTable[64] = {
    0,   0,   0,   0,   0,   0,  0,   0,
   98, 134,  61,  95,  68, 126, 34, -11,
   -6,   7,  26,  31,  65,  56, 25, -20,
  -14,  13,   6,  21,  23,  12, 17, -23,
  -27,  -2,  -5,  12,  17,   6, 10, -25,
  -26,  -4,  -4, -10,   3,   3, 33, -12,
  -35,  -1, -20, -23, -15,  24, 38, -22,
    0,   0,   0,   0,   0,   0,  0,   0,
};

int endPawnTable[64] = {
    0,   0,   0,   0,   0,   0,   0,   0,
  178, 173, 158, 134, 147, 132, 165, 187,
   94, 100,  85,  67,  56,  53,  82,  84,
   32,  24,  13,   5,  -2,   4,  17,  17,
   13,   9,  -3,  -7,  -7,  -8,   3,  -1,
    4,   7,  -6,   1,   0,  -5,  -1,  -8,
   13,   8,   8,  10,  13,   0,   2,  -7,
    0,   0,   0,   0,   0,   0,   0,   0,
};

int midKnightTable[64] = {
  -167, -89, -34, -49,  61, -97, -15, -107,
   -73, -41,  72,  36,  23,  62,   7,  -17,
   -47,  60,  37,  65,  84, 129,  73,   44,
    -9,  17,  19,  53,  37,  69,  18,   22,
   -13,   4,  16,  13,  28,  19,  21,   -8,
   -23,  -9,  12,  10,  19,  17,  25,  -16,
   -29, -53, -12,  -3,  -1,  18, -14,  -19,
  -105, -21, -58, -33, -17, -28, -19,  -23,
};

int endKnightTable[64] = {
  -58, -38, -13, -28, -31, -27, -63, -99,
  -25,  -8, -25,  -2,  -9, -25, -24, -52,
  -24, -20,  10,   9,  -1,  -9, -19, -41,
  -17,   3,  22,  22,  22,  11,   8, -18,
  -18,  -6,  16,  25,  16,  17,   4, -18,
  -23,  -3,  -1,  15,  10,  -3, -20, -22,
  -42, -20, -10,  -5,  -2, -20, -23, -44,
  -29, -51, -23, -15, -22, -18, -50, -64,
};

int midBishopTable[64] = {
  -29,   4, -82, -37, -25, -42,   7,  -8,
  -26,  16, -18, -13,  30,  59,  18, -47,
  -16,  37,  43,  40,  35,  50,  37,  -2,
   -4,   5,  19,  50,  37,  37,   7,  -2,
   -6,  13,  13,  26,  34,  12,  10,   4,
    0,  15,  15,  15,  14,  27,  18,  10,
    4,  15,  16,   0,   7,  21,  33,   1,
  -33,  -3, -14, -21, -13, -12, -39, -21,
};

int endBishopTable[64] = {
  -14, -21, -11,  -8, -7,  -9, -17, -24,
   -8,  -4,   7, -12, -3, -13,  -4, -14,
    2,  -8,   0,  -1, -2,   6,   0,   4,
   -3,   9,  12,   9, 14,  10,   3,   2,
   -6,   3,  13,  19,  7,  10,  -3,  -9,
  -12,  -3,   8,  10, 13,   3,  -7, -15,
  -14, -18,  -7,  -1,  4,  -9, -15, -27,
  -23,  -9, -23,  -5, -9, -16,  -5, -17,
};

int midRookTable[64] = {
   32,  42,  32,  51, 63,  9,  31,  43,
   27,  32,  58,  62, 80, 67,  26,  44,
   -5,  19,  26,  36, 17, 45,  61,  16,
  -24, -11,   7,  26, 24, 35,  -8, -20,
  -36, -26, -12,  -1,  9, -7,   6, -23,
  -45, -25, -16, -17,  3,  0,  -5, -33,
  -44, -16, -20,  -9, -1, 11,  -6, -71,
  -19, -13,   1,  17, 16,  7, -37, -26,
};

int endRookTable[64] = {
  13, 10, 18, 15, 12,  12,   8,   5,
  11, 13, 13, 11, -3,   3,   8,   3,
   7,  7,  7,  5,  4,  -3,  -5,  -3,
   4,  3, 13,  1,  2,   1,  -1,   2,
   3,  5,  8,  4, -5,  -6,  -8, -11,
  -4,  0, -5, -1, -7, -12,  -8, -16,
  -6, -6,  0,  2, -9,  -9, -11,  -3,
  -9,  2,  3, -1, -5, -13,   4, -20,
};

int midQueenTable[64] = {
  -28,   0,  29,  12,  59,  44,  43,  45,
  -24, -39,  -5,   1, -16,  57,  28,  54,
  -13, -17,   7,   8,  29,  56,  47,  57,
  -27, -27, -16, -16,  -1,  17,  -2,   1,
   -9, -26,  -9, -10,  -2,  -4,   3,  -3,
  -14,   2, -11,  -2,  -5,   2,  14,   5,
  -35,  -8,  11,   2,   8,  15,  -3,   1,
   -1, -18,  -9,  10, -15, -25, -31, -50,
};

int endQueenTable[64] = {
   -9,  22,  22,  27,  27,  19,  10,  20,
  -17,  20,  32,  41,  58,  25,  30,   0,
  -20,   6,   9,  49,  47,  35,  19,   9,
    3,  22,  24,  45,  57,  40,  57,  36,
  -18,  28,  19,  47,  31,  34,  39,  23,
  -16, -27,  15,   6,   9,  17,  10,   5,
  -22, -23, -30, -16, -16, -23, -36, -32,
  -33, -28, -22, -43,  -5, -32, -20, -41,
};

int midKingTable[64] = {
  -65,  23,  16, -15, -56, -34,   2,  13,
   29,  -1, -20,  -7,  -8,  -4, -38, -29,
   -9,  24,   2, -16, -20,   6,  22, -22,
  -17, -20, -12, -27, -30, -25, -14, -36,
  -49,  -1, -27, -39, -46, -44, -33, -51,
  -14, -14, -22, -46, -44, -30, -15, -27,
    1,   7,  -8, -64, -43, -16,   9,   8,
  -15,  36,  12, -54,   8, -28,  24,  14,
};

int endKingTable[64] = {
  -74, -35, -18, -18, -11,  15,   4, -17,
  -12,  17,  14,  17,  17,  38,  23,  11,
   10,  17,  23,  15,  20,  45,  44,  13,
   -8,  22,  24,  27,  26,  33,  26,   3,
  -18,  -4,  21,  24,  27,  23,   9, -11,
  -19,  -3,  11,  21,  23,  16,   7,  -9,
  -27, -11,   4,  13,  14,   4,  -5, -17,
  -53, -34, -21, -11, -28, -14, -24, -43
};


/*------------------------------------------------------------------------
    Visualize a bitboard
------------------------------------------------------------------------*/
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

/*------------------------------------------------------------------------
    Return game phase 0-24 for endgame to opening
------------------------------------------------------------------------*/
int gamePhase (const Board& board) {
    int phase = board.pieces(PieceType::KNIGHT, Color::WHITE).count() + board.pieces(PieceType::KNIGHT, Color::BLACK).count() +
                     board.pieces(PieceType::BISHOP, Color::WHITE).count() + board.pieces(PieceType::BISHOP, Color::BLACK).count() +
                     board.pieces(PieceType::ROOK, Color::WHITE).count() * 2 + board.pieces(PieceType::ROOK, Color::BLACK).count() * 2 +
                     board.pieces(PieceType::QUEEN, Color::WHITE).count() * 4 + board.pieces(PieceType::QUEEN, Color::BLACK).count() * 4;

    return phase;
}


/*------------------------------------------------------------------------
    Known draw.
------------------------------------------------------------------------*/
bool knownDraw(const Board& board) {

    // Two kings are a draw
    if (board.us(Color::WHITE).count() == 1 && board.us(Color::BLACK).count() == 1) {
        return true;
    }

    int whitePawnCount = board.pieces(PieceType::PAWN, Color::WHITE).count();
    int whiteKnightCount = board.pieces(PieceType::KNIGHT, Color::WHITE).count();
    int whiteBishopCount = board.pieces(PieceType::BISHOP, Color::WHITE).count();
    int whiteRookCount = board.pieces(PieceType::ROOK, Color::WHITE).count();
    int whiteQueenCount = board.pieces(PieceType::QUEEN, Color::WHITE).count();

    int blackPawnCount = board.pieces(PieceType::PAWN, Color::BLACK).count();
    int blackKnightCount = board.pieces(PieceType::KNIGHT, Color::BLACK).count();
    int blackBishopCount = board.pieces(PieceType::BISHOP, Color::BLACK).count();
    int blackRookCount = board.pieces(PieceType::ROOK, Color::BLACK).count();
    int blackQueenCount = board.pieces(PieceType::QUEEN, Color::BLACK).count();

    // If there are pawns on the board, it is not a draw
    if (whitePawnCount > 0 || blackPawnCount > 0) {
        return false;
    }

    // Cannot checkmate with 2 knights or a knight 
    if (whitePawnCount == 0 && whiteKnightCount <= 2 && whiteBishopCount == 0 && whiteRookCount == 0 && whiteQueenCount == 0 &&
        blackPawnCount == 0 && blackKnightCount <= 0 && blackBishopCount == 0 && blackRookCount == 0 && blackQueenCount == 0) {
        return true;
    }

    if (whitePawnCount == 0 && whiteKnightCount == 0 && whiteBishopCount == 0 && whiteRookCount == 0 && whiteQueenCount == 0 &&
        blackPawnCount == 0 && blackKnightCount <= 2 && blackBishopCount == 0 && blackRookCount == 0 && blackQueenCount == 0) {
        return true;
    }

    // Cannot checkmate with one bishop
    if (whitePawnCount == 0 && whiteKnightCount == 0 && whiteBishopCount == 1 && whiteRookCount == 0 && whiteQueenCount == 0 &&
        blackPawnCount == 0 && blackKnightCount == 0 && blackBishopCount == 0 && blackRookCount == 0 && blackQueenCount == 0) {
        return true;
    }

    if (whitePawnCount == 0 && whiteKnightCount == 0 && whiteBishopCount == 0 && whiteRookCount == 0 && whiteQueenCount == 0 &&
        blackPawnCount == 0 && blackKnightCount == 0 && blackBishopCount == 1 && blackRookCount == 0 && blackQueenCount == 0) {
        return true;
    }

    // A RK vs RB or a QK vs QB endgame is drawish
    if (whitePawnCount == 0 && whiteKnightCount == 0 && whiteBishopCount == 0 && whiteRookCount == 1 && whiteQueenCount == 0 &&
        blackPawnCount == 0 && blackKnightCount == 1 && blackBishopCount == 0 && blackRookCount == 0 && blackQueenCount == 0) {
        return true;
    }

    if (whitePawnCount == 0 && whiteKnightCount == 0 && whiteBishopCount == 0 && whiteRookCount == 1 && whiteQueenCount == 0 &&
        blackPawnCount == 0 && blackKnightCount == 0 && blackBishopCount == 1 && blackRookCount == 0 && blackQueenCount == 0) {
        return true;
    }

    if (whitePawnCount == 0 && whiteKnightCount == 1 && whiteBishopCount == 0 && whiteRookCount == 0 && whiteQueenCount == 0 &&
        blackPawnCount == 0 && blackKnightCount == 0 && blackBishopCount == 0 && blackRookCount == 1 && blackQueenCount == 0) {
        return true;
    }

    if (whitePawnCount == 0 && whiteKnightCount == 0 && whiteBishopCount == 1 && whiteRookCount == 0 && whiteQueenCount == 0 &&
        blackPawnCount == 0 && blackKnightCount == 0 && blackBishopCount == 0 && blackRookCount == 1 && blackQueenCount == 0) {
        return true;
    }

    return false;

}


/*------------------------------------------------------------------------
    Calculate material imbalance in centipawn
------------------------------------------------------------------------*/
int materialImbalance(const Board& board) {
    Bitboard whitePawns = board.pieces(PieceType::PAWN, Color::WHITE);
    Bitboard whiteKnights = board.pieces(PieceType::KNIGHT, Color::WHITE);
    Bitboard whiteBishops = board.pieces(PieceType::BISHOP, Color::WHITE);
    Bitboard whiteRooks = board.pieces(PieceType::ROOK, Color::WHITE);
    Bitboard whiteQueens = board.pieces(PieceType::QUEEN, Color::WHITE);

    Bitboard blackPawns = board.pieces(PieceType::PAWN, Color::BLACK);
    Bitboard blackKnights = board.pieces(PieceType::KNIGHT, Color::BLACK);
    Bitboard blackBishops = board.pieces(PieceType::BISHOP, Color::BLACK);
    Bitboard blackRooks = board.pieces(PieceType::ROOK, Color::BLACK);
    Bitboard blackQueens = board.pieces(PieceType::QUEEN, Color::BLACK);

    int whiteMaterial = whitePawns.count() * PAWN_VALUE +
                        whiteKnights.count() * KNIGHT_VALUE +
                        whiteBishops.count() * BISHOP_VALUE +
                        whiteRooks.count() * ROOK_VALUE +
                        whiteQueens.count() * QUEEN_VALUE;

    int blackMaterial = blackPawns.count() * PAWN_VALUE + 
                        blackKnights.count() * KNIGHT_VALUE +
                        blackBishops.count() * BISHOP_VALUE +
                        blackRooks.count() * ROOK_VALUE +
                        blackQueens.count() * QUEEN_VALUE;

    return whiteMaterial - blackMaterial;
}

/*------------------------------------------------------------------------
    Check if the given square is a passed pawn 
------------------------------------------------------------------------*/
bool isPassedPawn(int sqIndex, Color color, const Bitboard& theirPawns) {
    int file = sqIndex % 8;
    int rank = sqIndex / 8;

    Bitboard theirPawnsCopy = theirPawns;

    while (theirPawnsCopy) {
        int sqIndex2 = theirPawnsCopy.lsb();  
        int file2 = sqIndex2 % 8;
        int rank2 = sqIndex2 / 8;

        if (std::abs(file - file2) <= 1 && rank2 > rank && color == Color::WHITE) {
            return false; 
        }

        if (std::abs(file - file2) <= 1 && rank2 < rank && color == Color::BLACK) {
            return false; 
        }

        theirPawnsCopy.clear(sqIndex2);
    }

    return true;  
}

/*------------------------------------------------------------------------
    Manhattan distance between two squares
------------------------------------------------------------------------*/
int manhattanDistance(const Square& sq1, const Square& sq2) {
    return std::abs(sq1.file() - sq2.file()) + std::abs(sq1.rank() - sq2.rank());
}

/*------------------------------------------------------------------------
    Min distance between two squares
------------------------------------------------------------------------*/
int minDistance(const Square& sq, const Square& sq2) {
    return std::min(std::abs(sq.file() - sq2.file()), std::abs(sq.rank() - sq2.rank()));
}

/*------------------------------------------------------------------------
    Min distance to the edge of the board
------------------------------------------------------------------------*/
int minDistanceToEdge(const Square& sq) {
    int file = sq.index() % 8;
    int rank = sq.index() / 8;
    return std::min(std::min(file, 7 - file), std::min(rank, 7 - rank));
}

/*------------------------------------------------------------------------
    Mop up score. This function assume the board is in mop up phase.
------------------------------------------------------------------------*/
int mopUpScore(const Board& board) {

    int whitePawnsCount = board.pieces(PieceType::PAWN, Color::WHITE).count();
    int blackPawnsCount = board.pieces(PieceType::PAWN, Color::BLACK).count();

    int whiteKnightsCount = board.pieces(PieceType::KNIGHT, Color::WHITE).count();
    int blackKnightsCount = board.pieces(PieceType::KNIGHT, Color::BLACK).count();

    int whiteBishopsCount = board.pieces(PieceType::BISHOP, Color::WHITE).count();
    int blackBishopsCount = board.pieces(PieceType::BISHOP, Color::BLACK).count();

    int whiteRooksCount = board.pieces(PieceType::ROOK, Color::WHITE).count();
    int blackRooksCount = board.pieces(PieceType::ROOK, Color::BLACK).count();

    int whiteQueensCount = board.pieces(PieceType::QUEEN, Color::WHITE).count();
    int blackQueensCount = board.pieces(PieceType::QUEEN, Color::BLACK).count();

    const int whiteMaterial = whitePawnsCount 
                            + whiteKnightsCount * 3 
                            + whiteBishopsCount * 3 
                            + whiteRooksCount * 5 
                            + whiteQueensCount * 10;
    const int blackMaterial = blackPawnsCount 
                            + blackKnightsCount * 3 
                            + blackBishopsCount * 3 
                            + blackRooksCount * 5 
                            + blackQueensCount * 10;   

    Color winningColor = whiteMaterial > blackMaterial ? Color::WHITE : Color::BLACK;

    Bitboard pieces = board.pieces(PieceType::PAWN, winningColor) | board.pieces(PieceType::KNIGHT, winningColor) | 
                    board.pieces(PieceType::BISHOP, winningColor) | board.pieces(PieceType::ROOK, winningColor) | 
                    board.pieces(PieceType::QUEEN, winningColor);


    Square winningKingSq = Square(board.pieces(PieceType::KING, winningColor).lsb());
    Square losingKingSq = Square(board.pieces(PieceType::KING, !winningColor).lsb());
    int losingKingSqIndex = losingKingSq.index();

    int kingDist = manhattanDistance(winningKingSq, losingKingSq);

    int winningMaterialScore = winningColor == Color::WHITE ? whiteMaterial : blackMaterial;
    int losingMaterialScore = winningColor == Color::WHITE ? blackMaterial : whiteMaterial;
    int materialScore = 100 * (winningMaterialScore - losingMaterialScore);

    bool bnMate = (winningColor == Color::WHITE && whiteQueensCount == 0 && whiteRooksCount == 0 && whiteBishopsCount == 1 && whiteKnightsCount == 1) 
                || (winningColor == Color::BLACK && blackQueensCount == 0 && blackRooksCount == 0 && blackBishopsCount == 1 && blackKnightsCount == 1);

    int e4 = 28;
    int a1 = 0;
    int h1 = 7;
    int a8 = 56;
    int h8 = 63;
    
    if (bnMate) {
        Bitboard bishop;
        if (winningColor == Color::WHITE) {
            bishop = board.pieces(PieceType::BISHOP, Color::WHITE);
        } else {
            bishop = board.pieces(PieceType::BISHOP, Color::BLACK);
        }

        Square bishopSq = Square(bishop.lsb());
        int bishopRank = bishop.lsb() / 8;
        int bishopFile = bishop.lsb() % 8;

        bool darkSquareBishop = (bishopRank + bishopFile) % 2 == 0;

        // int cornerDist;

        // if (darkSquareBishop) {
        //     cornerDist = manhattanDistance(losingKingSq, Square(a1));
        // } else {
        //     cornerDist = manhattanDistance(losingKingSq, Square(a8));
        // }

        const int *bnMateTable = darkSquareBishop ? bnMateDarkSquares : bnMateLightSquares;

        int score = 5000 + materialScore + 150 * (14 - kingDist) + materialScore + 100 * bnMateTable[losingKingSqIndex];

        return winningColor == Color::WHITE ? score : -score;
    }

    int score = 5000 + 150 * (14 - kingDist) + materialScore + 475 * manhattanDistance(losingKingSq, Square(e4)) ;
    
    return winningColor == Color::WHITE ? score : -score;
    
    return 0;
}

int moveScoreByTable(const Board& board, Move move) {
    PieceType pieceType = board.at<Piece>(move.from()).type();
    Color color = board.at<Piece>(move.from()).color();

    int destinationIndex = move.to().index();
    int phase = gamePhase(board);

    double midWeight = static_cast<double> (phase / 24.0);
    double endWeight = 1 - midWeight;

    if (color == Color::BLACK) {
        destinationIndex = (7 - (destinationIndex / 8)) * 8 + (destinationIndex % 8);
    }

    if (pieceType == PieceType::PAWN) {
        return midWeight * midPawnTable[destinationIndex] + endWeight * endPawnTable[destinationIndex];
    } else if (pieceType == PieceType::KNIGHT) {
        return midWeight * midKnightTable[destinationIndex] + endWeight * endKnightTable[destinationIndex];
    } else if (pieceType == PieceType::BISHOP) {
        return midWeight * midBishopTable[destinationIndex] + endWeight * endBishopTable[destinationIndex];
    } else if (pieceType == PieceType::ROOK) {
        return midWeight * midRookTable[destinationIndex] + endWeight * endRookTable[destinationIndex];
    } else if (pieceType == PieceType::QUEEN) {
        return midWeight * midQueenTable[destinationIndex] + endWeight * endQueenTable[destinationIndex];
    } else if (pieceType == PieceType::KING) {
        return midWeight * midKingTable[destinationIndex] + endWeight * endKingTable[destinationIndex];
    }

    return 0;
}


/*-------------------------------------------------------------------------------------------- 
    mopUp Phase
--------------------------------------------------------------------------------------------*/
bool isMopUpPhase(Board& board) {
    int whitePawnsCount = board.pieces(PieceType::PAWN, Color::WHITE).count();
    int blackPawnsCount = board.pieces(PieceType::PAWN, Color::BLACK).count();

    int whiteKnightsCount = board.pieces(PieceType::KNIGHT, Color::WHITE).count();
    int blackKnightsCount = board.pieces(PieceType::KNIGHT, Color::BLACK).count();

    int whiteBishopsCount = board.pieces(PieceType::BISHOP, Color::WHITE).count();
    int blackBishopsCount = board.pieces(PieceType::BISHOP, Color::BLACK).count();

    int whiteRooksCount = board.pieces(PieceType::ROOK, Color::WHITE).count();
    int blackRooksCount = board.pieces(PieceType::ROOK, Color::BLACK).count();

    int whiteQueensCount = board.pieces(PieceType::QUEEN, Color::WHITE).count();
    int blackQueensCount = board.pieces(PieceType::QUEEN, Color::BLACK).count();

    const int whiteMaterial = whitePawnsCount + whiteKnightsCount * 3 + whiteBishopsCount * 3 + whiteRooksCount * 5 + whiteQueensCount * 10;
    const int blackMaterial = blackPawnsCount + blackKnightsCount * 3 + blackBishopsCount * 3 + blackRooksCount * 5 + blackQueensCount * 10;    

    if (whitePawnsCount > 0 || blackPawnsCount > 0) {
        // if there are pawns, it's not a settled
        return false;
    } else if (std::abs(whiteMaterial - blackMaterial) > 4) {
        // This covers cases such as KQvK, KRvK, KQvKR, KBBvK
        // Less obvious cases are also covered , KR+minor pieces v K+minor, 
        return true;
    }

    // Otherwise, if we have K + a minor piece, or KR vs K + minor piece, it's drawish
    return false;
}


/*-------------------------------------------------------------------------------------------- 
    Check for tactical threats beside the obvious checks, captures, and promotions.
    To be expanded. 
--------------------------------------------------------------------------------------------*/
bool mateThreatMove(Board& board, Move move) {
    Color color = board.sideToMove();
    PieceType type = board.at<Piece>(move.from()).type();

    Bitboard theirKing = board.pieces(PieceType::KING, !color);

    int destinationIndex = move.to().index();   
    int destinationFile = destinationIndex % 8;
    int destinationRank = destinationIndex / 8;

    int theirKingFile = theirKing.lsb() % 8;
    int theirKingRank = theirKing.lsb() / 8;

    if (manhattanDistance(move.to(), Square(theirKing.lsb())) <= 3) {
        return true;
    }

    if (type == PieceType::ROOK || type == PieceType::QUEEN) {
        if (abs(destinationFile - theirKingFile) <= 1 && 
            abs(destinationRank - theirKingRank) <= 1) {
            return true;
        }
    }

    return false;
}