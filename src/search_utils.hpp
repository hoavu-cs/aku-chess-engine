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

const int bnMateLightSquares[64] = {
    0, 10, 20, 30, 40, 50, 60, 70,
    10, 20, 30, 40, 50, 60, 70, 60,
    20, 30, 40, 50, 60, 70, 60, 50,
    30, 40, 50, 60, 70, 60, 50, 40,
    40, 50, 60, 70, 60, 50, 40, 30,
    50, 60, 70, 60, 50, 40, 30, 20,
    60, 70, 60, 50, 40, 30, 20, 10,
    70, 60, 50, 40, 30, 20, 10, 0
};

const int bnMateDarkSquares[64] = {
    70, 60, 50, 40, 30, 20, 10, 0,
    60, 70, 60, 50, 40, 30, 20, 10,
    50, 60, 70, 60, 50, 40, 30, 20,
    40, 50, 60, 70, 60, 50, 40, 30,
    30, 40, 50, 60, 70, 60, 50, 40,
    20, 30, 40, 50, 60, 70, 60, 50,
    10, 20, 30, 40, 50, 60, 70, 60,
    0, 10, 20, 30, 40, 50, 60, 70
};

//--------------------------------------------------------------------------------------------- 
//Function declarations
//---------------------------------------------------------------------------------------------

// gamephase 24: beginning, 16: middle, 12: endgame
int gamePhase(const Board& board);

// manhattan distance between two squares
int manhattanDistance(const Square& sq1, const Square& sq2);

/// Distance between two squares
int minDistance(const Square& sq1, const Square& sq2);

// Distance to the edge of the board
int minDistanceToEdge(const Square& sq);

// mopUp Phase: Looking for checkmate phase
bool isMopUpPhase(Board& board);

// checkmate score (i.e., drive opponent's king to the edge of the board)
int mopUpScore(const Board& board);


//--------------------------------------------------------------------------------------------- 
//Function definitions
//---------------------------------------------------------------------------------------------

int gamePhase (const Board& board) {
    int phase = board.pieces(PieceType::KNIGHT, Color::WHITE).count() + board.pieces(PieceType::KNIGHT, Color::BLACK).count() +
                     board.pieces(PieceType::BISHOP, Color::WHITE).count() + board.pieces(PieceType::BISHOP, Color::BLACK).count() +
                     board.pieces(PieceType::ROOK, Color::WHITE).count() * 2 + board.pieces(PieceType::ROOK, Color::BLACK).count() * 2 +
                     board.pieces(PieceType::QUEEN, Color::WHITE).count() * 4 + board.pieces(PieceType::QUEEN, Color::BLACK).count() * 4;

    return phase;
}

int manhattanDistance(const Square& sq1, const Square& sq2) {
    return std::abs(sq1.file() - sq2.file()) + std::abs(sq1.rank() - sq2.rank());
}

int minDistance(const Square& sq, const Square& sq2) {
    return std::min(std::abs(sq.file() - sq2.file()), std::abs(sq.rank() - sq2.rank()));
}

int minDistanceToEdge(const Square& sq) {
    int file = sq.index() % 8;
    int rank = sq.index() / 8;
    return std::min(std::min(file, 7 - file), std::min(rank, 7 - rank));
}

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

inline bool isPassedPawn(int sqIndex, Color color, const Bitboard& theirPawns) {
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

inline bool promotionThreat(Board& board, Move move) {
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