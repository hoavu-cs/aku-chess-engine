#include "chess.hpp"
#include "evaluation.hpp"

using namespace chess; 

const int PAWN_VALUE = 120;
const int KNIGHT_VALUE = 320; 
const int BISHOP_VALUE = 330;
const int ROOK_VALUE = 500;
const int QUEEN_VALUE = 900;
const int KING_VALUE = 5000;

/*------------------------------------------------------------------------
    Helper Functions
------------------------------------------------------------------------*/

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

// Function to compute the Manhattan distance between two squares
int manhattanDistance(const Square& sq1, const Square& sq2) {
    return std::abs(sq1.file() - sq2.file()) + std::abs(sq1.rank() - sq2.rank());
}

// Min distance between a square and a set of squares
int minDistance(const Square& sq, const Square& sq2) {
    return std::min(std::abs(sq.file() - sq2.file()), std::abs(sq.rank() - sq2.rank()));
}

/*------------------------------------------------------------------------
    Mop up score for KQ v K, KR v K
------------------------------------------------------------------------*/
int mopUpScore(const Board& board) {
    if (board.us(Color::WHITE).count() == 1 && board.us(Color::BLACK).count() == 1) {
        return 0; 
    }

    bool mopUp = board.us(Color::WHITE).count() == 1 || board.us(Color::BLACK).count() == 1;

    Color winningColor = board.us(Color::WHITE).count() > 1 ? Color::WHITE : Color::BLACK;

    Bitboard pieces = board.pieces(PieceType::PAWN, winningColor) | board.pieces(PieceType::KNIGHT, winningColor) | 
                    board.pieces(PieceType::BISHOP, winningColor) | board.pieces(PieceType::ROOK, winningColor) | 
                    board.pieces(PieceType::QUEEN, winningColor);

    if (mopUp) {
        Square winningKingSq = Square(board.pieces(PieceType::KING, winningColor).lsb());
        Square losingKingSq = Square(board.pieces(PieceType::KING, !winningColor).lsb());
        Square E4 = Square(28);

        int kingDist = manhattanDistance(winningKingSq, losingKingSq);
        int distToCenter = manhattanDistance(losingKingSq, E4);
        int winningMaterial = 900 * board.pieces(PieceType::QUEEN, winningColor).count() + 
                          500 * board.pieces(PieceType::ROOK, winningColor).count() + 
                          300 * board.pieces(PieceType::BISHOP, winningColor).count() + 
                          300 * board.pieces(PieceType::KNIGHT, winningColor).count() + 
                          100 * board.pieces(PieceType::PAWN, winningColor).count(); // avoid throwing away pieces
        int score = 5000 +  500 * distToCenter + 150 * (14 - kingDist);

        return winningColor == Color::WHITE ? score : -score;
    }

    return 0;
}