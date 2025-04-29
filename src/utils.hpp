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

/*------------------------------------------------------------------------------
    Small utility functions for the engine.

    void bitBoardVisualize(const Bitboard& board);

    int gamePhase(const Board& board);

    bool knownDraw(const Board& board);

    int materialImbalance(const Board& board);

    bool isPassedPawn(int sqIndex, Color color, const Bitboard& theirPawns);

    int manhattanDistance(const Square& sq1, const Square& sq2);

    int minDistance(const Square& sq, const Square& sq2);

    int mopUpScore(const Board& board);

    bool isMopUpPhase(Board& board);

    inline bool promotionThreatMove(Board& board, Move move);

    std::string formatAnalysis(
        int depth,
        int bestEval,
        size_t totalNodeCount,
        size_t totalTableHit,
        const std::chrono::high_resolution_clock::time_point& startTime,
        const std::vector<Move>& PV,
        const Board& board); 

    inline uint32_t fastRand(uint32_t& seed);
------------------------------------------------------------------------------*/

#include "chess.hpp"
#include <chrono>

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
        // Typically not needed thanks to Syzygy tablebase.
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
        const int *bnMateTable = darkSquareBishop ? bnMateDarkSquares : bnMateLightSquares;
        int score = 5000 + materialScore + 150 * (14 - kingDist) + materialScore + 100 * bnMateTable[losingKingSqIndex];
        return winningColor == Color::WHITE ? score : -score;
    }

    int score = 5000 + 150 * (14 - kingDist) + materialScore + 475 * manhattanDistance(losingKingSq, Square(e4)) ;
    return winningColor == Color::WHITE ? score : -score;
    
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
        return true;
    }

    // Otherwise, if we have K + a minor piece, or KR vs K + minor piece, it's drawish
    return false;
}


/*-------------------------------------------------------------------------------------------- 
    Check if the move involves a passed pawn push.
--------------------------------------------------------------------------------------------*/
inline bool promotionThreatMove(Board& board, Move move) {
    Color color = board.sideToMove();
    PieceType type = board.at<Piece>(move.from()).type();

    if (type == PieceType::PAWN) {
        int destinationIndex = move.to().index();
        int rank = destinationIndex / 8;
        Bitboard theirPawns = board.pieces(PieceType::PAWN, !color);
        bool isPassedPawnFlag = isPassedPawn(destinationIndex, color, theirPawns);

        if (isPassedPawnFlag) {
            if ((color == Color::WHITE && rank > 3) || 
                (color == Color::BLACK && rank < 4)) {
                return true;
            }
        }
    }

    return false;
}

/*-------------------------------------------------------------------------------------------- 
    Return the UCI analysis string for the given parameters.
--------------------------------------------------------------------------------------------*/
std::string formatAnalysis(
    int depth,
    int bestEval,
    size_t totalNodeCount,
    size_t totalTableHit,
    const std::chrono::high_resolution_clock::time_point& startTime,
    const std::vector<Move>& PV,
    const Board& board
) {
    std::string depthStr = "depth " + std::to_string(std::max(size_t(depth), PV.size()));
    std::string scoreStr = "score cp " + std::to_string(bestEval);
    std::string nodeStr = "nodes " + std::to_string(totalNodeCount);
    std::string tableHitStr = "tableHit " + std::to_string(
        static_cast<double>(totalTableHit) / totalNodeCount
    );

    auto iterationEndTime = std::chrono::high_resolution_clock::now();
    std::string timeStr = "time " + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(iterationEndTime - startTime).count()
    );

    std::string pvStr = "pv ";
    for (const auto& move : PV) {
        pvStr += uci::moveToUci(move, board.chess960()) + " ";
    }

    std::string analysis = "info " + depthStr + " " + scoreStr + " " + nodeStr + " " + timeStr + " " + pvStr;
    return analysis;
}


/*-------------------------------------------------------------------------------------------- 
    Fast random number generator.
--------------------------------------------------------------------------------------------*/
inline uint32_t fastRand(uint32_t& seed) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}