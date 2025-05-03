#include "chess.hpp"
#include "search.hpp"
#include <chrono>

using namespace chess; 

typedef uint64_t U64;

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

// Function declarations
inline void evalAdjust(int& eval);
inline int gamePhase(const Board& board);
inline int manhattanDistance(const Square& sq1, const Square& sq2);
inline int minDistance(const Square& sq1, const Square& sq2);
inline int minDistanceToEdge(const Square& sq);
inline U64 moveIndex(const Move& move);
inline bool isCastling(const Move& move);
inline bool isPromotion(const Move& move);
inline bool isMopUpPhase(Board& board);
inline int mopUpScore(const Board& board);
inline void updatePV(std::vector<Move>& PV, const Move& move, const std::vector<Move>& childPV);
inline bool promotionThreat(Board& board, Move move);


// Function definitions
inline void evalAdjust(int& eval) {
    if (eval >= INF/2 - 100) {
        eval--; 
    } else if (eval <= -INF/2 + 100) {
        eval++; 
    } else if (eval >= SZYZYGY_INF - 100) {
        eval--; 
    } else if (eval <= -SZYZYGY_INF + 100) {
        eval++;
    }
}

inline void updatePV(std::vector<Move>& PV, const Move& move, const std::vector<Move>& childPV) {
    PV.clear();
    PV.push_back(move);
    PV.insert(PV.end(), childPV.begin(), childPV.end());
}

inline int gamePhase (const Board& board) {
    return board.pieces(PieceType::KNIGHT).count() +
                board.pieces(PieceType::BISHOP).count() +
                board.pieces(PieceType::ROOK).count() * 2 +
                board.pieces(PieceType::QUEEN).count() * 4;
}

inline int manhattanDistance(const Square& sq1, const Square& sq2) {
    return std::abs(sq1.file() - sq2.file()) + std::abs(sq1.rank() - sq2.rank());
}

inline int minDistance(const Square& sq, const Square& sq2) {
    return std::min(std::abs(sq.file() - sq2.file()), std::abs(sq.rank() - sq2.rank()));
}

inline int minDistanceToEdge(const Square& sq) {
    int file = sq.index() % 8;
    int rank = sq.index() / 8;
    return std::min(std::min(file, 7 - file), std::min(rank, 7 - rank));
}

inline U64 moveIndex(const Move& move) {
    return move.from().index() * 64 + move.to().index();
}

inline bool isCastling(const Move& move) {
    if (move.typeOf() & Move::CASTLING) {
        return true;
    } 
    return false;
}

inline bool isPromotion(const Move& move) {
    if (move.typeOf() & Move::PROMOTION) {
        if (move.promotionType() == PieceType::QUEEN) {
            return true;
        } 
    } 
    return false;
}

inline bool isPassedPawn(int sqIndex, Color color, const Bitboard& theirPawns) {
    int file = sqIndex % 8;
    int rank = sqIndex / 8;
    Bitboard copy = theirPawns;
    while (copy) {
        int sqIndex2 = copy.lsb(), file2 = sqIndex2 % 8, rank2 = sqIndex2 / 8;
        if (std::abs(file - file2) <= 1 && rank2 > rank && color == Color::WHITE) 
            return false; 
        if (std::abs(file - file2) <= 1 && rank2 < rank && color == Color::BLACK) 
            return false; 
        copy.clear(sqIndex2);
    }
    return true;  
}

inline bool promotionThreat(Board& board, Move move) {
    if (board.at(move.from()).type() != PieceType::PAWN) return false;

    Color color = board.sideToMove();
    PieceType type = board.at<Piece>(move.from()).type();
    int destinationIndex = move.to().index();
    int toRank = destinationIndex / 8;
    Bitboard theirPawns = board.pieces(PieceType::PAWN, !color);
    bool isPassedPawnFlag = isPassedPawn(destinationIndex, color, theirPawns);
    if (isPassedPawnFlag) {
        if ((color == Color::WHITE && toRank > 3) || 
            (color == Color::BLACK && toRank < 4)) {
            return true;
        }
    }
}

inline bool isMopUpPhase(Board& board) {
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

inline int mopUpScore(const Board& board) {

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

