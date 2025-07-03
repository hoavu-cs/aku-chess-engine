#include "chess.hpp"
#include "search.h"
#include <chrono>

using namespace chess; 

typedef uint64_t U64;

const int PAWN_VALUE = 120;
const int KNIGHT_VALUE = 320; 
const int BISHOP_VALUE = 330;
const int ROOK_VALUE = 500;
const int QUEEN_VALUE = 900;
const int KING_VALUE = 0;

constexpr int bn_mate_light_squares[64] = {
    0, 10, 20, 30, 40, 50, 60, 70,
    10, 20, 30, 40, 50, 60, 70, 60,
    20, 30, 40, 50, 60, 70, 60, 50,
    30, 40, 50, 60, 70, 60, 50, 40,
    40, 50, 60, 70, 60, 50, 40, 30,
    50, 60, 70, 60, 50, 40, 30, 20,
    60, 70, 60, 50, 40, 30, 20, 10,
    70, 60, 50, 40, 30, 20, 10, 0
};

constexpr int bn_mate_dark_squares[64] = {
    70, 60, 50, 40, 30, 20, 10, 0,
    60, 70, 60, 50, 40, 30, 20, 10,
    50, 60, 70, 60, 50, 40, 30, 20,
    40, 50, 60, 70, 60, 50, 40, 30,
    30, 40, 50, 60, 70, 60, 50, 40,
    20, 30, 40, 50, 60, 70, 60, 50,
    10, 20, 30, 40, 50, 60, 70, 60,
    0, 10, 20, 30, 40, 50, 60, 70
};


constexpr int mid_pawn[64] = {
    0,   0,   0,   0,   0,   0,  0,   0,
   98, 134,  61,  95,  68, 126, 34, -11,
   -6,   7,  26,  31,  65,  56, 25, -20,
  -14,  13,   6,  21,  23,  12, 17, -23,
  -27,  -2,  -5,  12,  17,   6, 10, -25,
  -26,  -4,  -4, -10,   3,   3, 33, -12,
  -35,  -1, -20, -23, -15,  24, 38, -22,
    0,   0,   0,   0,   0,   0,  0,   0,
};

constexpr int end_pawn[64] = {
    0,   0,   0,   0,   0,   0,   0,   0,
  178, 173, 158, 134, 147, 132, 165, 187,
   94, 100,  85,  67,  56,  53,  82,  84,
   32,  24,  13,   5,  -2,   4,  17,  17,
   13,   9,  -3,  -7,  -7,  -8,   3,  -1,
    4,   7,  -6,   1,   0,  -5,  -1,  -8,
   13,   8,   8,  10,  13,   0,   2,  -7,
    0,   0,   0,   0,   0,   0,   0,   0,
};

constexpr int mid_knight[64] = {
  -167, -89, -34, -49,  61, -97, -15, -107,
   -73, -41,  72,  36,  23,  62,   7,  -17,
   -47,  60,  37,  65,  84, 129,  73,   44,
    -9,  17,  19,  53,  37,  69,  18,   22,
   -13,   4,  16,  13,  28,  19,  21,   -8,
   -23,  -9,  12,  10,  19,  17,  25,  -16,
   -29, -53, -12,  -3,  -1,  18, -14,  -19,
  -105, -21, -58, -33, -17, -28, -19,  -23,
};

constexpr int end_knight[64] = {
  -58, -38, -13, -28, -31, -27, -63, -99,
  -25,  -8, -25,  -2,  -9, -25, -24, -52,
  -24, -20,  10,   9,  -1,  -9, -19, -41,
  -17,   3,  22,  22,  22,  11,   8, -18,
  -18,  -6,  16,  25,  16,  17,   4, -18,
  -23,  -3,  -1,  15,  10,  -3, -20, -22,
  -42, -20, -10,  -5,  -2, -20, -23, -44,
  -29, -51, -23, -15, -22, -18, -50, -64,
};

constexpr int mid_bishop[64] = {
  -29,   4, -82, -37, -25, -42,   7,  -8,
  -26,  16, -18, -13,  30,  59,  18, -47,
  -16,  37,  43,  40,  35,  50,  37,  -2,
   -4,   5,  19,  50,  37,  37,   7,  -2,
   -6,  13,  13,  26,  34,  12,  10,   4,
    0,  15,  15,  15,  14,  27,  18,  10,
    4,  15,  16,   0,   7,  21,  33,   1,
  -33,  -3, -14, -21, -13, -12, -39, -21,
};

constexpr int end_bishop[64] = {
  -14, -21, -11,  -8, -7,  -9, -17, -24,
   -8,  -4,   7, -12, -3, -13,  -4, -14,
    2,  -8,   0,  -1, -2,   6,   0,   4,
   -3,   9,  12,   9, 14,  10,   3,   2,
   -6,   3,  13,  19,  7,  10,  -3,  -9,
  -12,  -3,   8,  10, 13,   3,  -7, -15,
  -14, -18,  -7,  -1,  4,  -9, -15, -27,
  -23,  -9, -23,  -5, -9, -16,  -5, -17,
};

constexpr int mid_rook[64] = {
   32,  42,  32,  51, 63,  9,  31,  43,
   27,  32,  58,  62, 80, 67,  26,  44,
   -5,  19,  26,  36, 17, 45,  61,  16,
  -24, -11,   7,  26, 24, 35,  -8, -20,
  -36, -26, -12,  -1,  9, -7,   6, -23,
  -45, -25, -16, -17,  3,  0,  -5, -33,
  -44, -16, -20,  -9, -1, 11,  -6, -71,
  -19, -13,   1,  17, 16,  7, -37, -26,
};

constexpr int end_rook[64] = {
  13, 10, 18, 15, 12,  12,   8,   5,
  11, 13, 13, 11, -3,   3,   8,   3,
   7,  7,  7,  5,  4,  -3,  -5,  -3,
   4,  3, 13,  1,  2,   1,  -1,   2,
   3,  5,  8,  4, -5,  -6,  -8, -11,
  -4,  0, -5, -1, -7, -12,  -8, -16,
  -6, -6,  0,  2, -9,  -9, -11,  -3,
  -9,  2,  3, -1, -5, -13,   4, -20,
};

constexpr int mid_queen[64] = {
  -28,   0,  29,  12,  59,  44,  43,  45,
  -24, -39,  -5,   1, -16,  57,  28,  54,
  -13, -17,   7,   8,  29,  56,  47,  57,
  -27, -27, -16, -16,  -1,  17,  -2,   1,
   -9, -26,  -9, -10,  -2,  -4,   3,  -3,
  -14,   2, -11,  -2,  -5,   2,  14,   5,
  -35,  -8,  11,   2,   8,  15,  -3,   1,
   -1, -18,  -9,  10, -15, -25, -31, -50,
};

constexpr int end_queen[64] = {
   -9,  22,  22,  27,  27,  19,  10,  20,
  -17,  20,  32,  41,  58,  25,  30,   0,
  -20,   6,   9,  49,  47,  35,  19,   9,
    3,  22,  24,  45,  57,  40,  57,  36,
  -18,  28,  19,  47,  31,  34,  39,  23,
  -16, -27,  15,   6,   9,  17,  10,   5,
  -22, -23, -30, -16, -16, -23, -36, -32,
  -33, -28, -22, -43,  -5, -32, -20, -41,
};

constexpr int mid_king[64] = {
  -65,  23,  16, -15, -56, -34,   2,  13,
   29,  -1, -20,  -7,  -8,  -4, -38, -29,
   -9,  24,   2, -16, -20,   6,  22, -22,
  -17, -20, -12, -27, -30, -25, -14, -36,
  -49,  -1, -27, -39, -46, -44, -33, -51,
  -14, -14, -22, -46, -44, -30, -15, -27,
    1,   7,  -8, -64, -43, -16,   9,   8,
  -15,  36,  12, -54,   8, -28,  24,  14,
};

constexpr int end_king[64] = {
  -74, -35, -18, -18, -11,  15,   4, -17,
  -12,  17,  14,  17,  17,  38,  23,  11,
   10,  17,  23,  15,  20,  45,  44,  13,
   -8,  22,  24,  27,  26,  33,  26,   3,
  -18,  -4,  21,  24,  27,  23,   9, -11,
  -19,  -3,  11,  21,  23,  16,   7,  -9,
  -27, -11,   4,  13,  14,   4,  -5, -17,
  -53, -34, -21, -11, -28, -14, -24, -43
};


// Function declarations
inline void eval_adjust(int& eval);
inline int game_phase(const Board& board);
inline int manhattan_distance(const Square& sq1, const Square& sq2);
inline int min_distance(const Square& sq1, const Square& sq2);
inline U64 move_index(const Move& move);
inline bool is_castling(const Move& move);
inline bool is_promotion(const Move& move);
inline bool is_mopup(Board& board);
inline int mopup_score(const Board& board);
inline void update_pv(std::vector<Move>& PV, const Move& move, const std::vector<Move>& childPV);
inline bool promotion_threat(Board& board, Move move);
inline bool non_pawn_material(Board& board);
inline int piece_type_value(PieceType pt);

// Function definitions
inline void eval_adjust(int& eval) {
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

inline void update_pv(std::vector<Move>& PV, const Move& move, const std::vector<Move>& childPV) {
    PV.clear();
    PV.push_back(move);
    PV.insert(PV.end(), childPV.begin(), childPV.end());
}

inline int game_phase (const Board& board) {
    return board.pieces(PieceType::KNIGHT).count() +
            board.pieces(PieceType::BISHOP).count() +
            board.pieces(PieceType::ROOK).count() * 2 +
            board.pieces(PieceType::QUEEN).count() * 4;
}

inline int manhattan_distance(const Square& sq1, const Square& sq2) {
    return std::abs(sq1.file() - sq2.file()) + std::abs(sq1.rank() - sq2.rank());
}

inline int min_distance(const Square& sq, const Square& sq2) {
    return std::min(std::abs(sq.file() - sq2.file()), std::abs(sq.rank() - sq2.rank()));
}

inline U64 move_index(const Move& move) {
    return move.from().index() * 64 + move.to().index();
}

inline bool is_castling(const Move& move) {
    if (move.typeOf() & Move::CASTLING) {
        return true;
    } 
    return false;
}

inline bool is_promotion(const Move& move) {
    if (move.typeOf() & Move::PROMOTION) {
        if (move.promotionType() == PieceType::QUEEN) {
            return true;
        } 
    } 
    return false;
}

inline bool is_passed_pawn(int sqIndex, Color color, const Bitboard& theirPawns) {
    int file = sqIndex % 8;
    int rank = sqIndex / 8;
    Bitboard copy = theirPawns;
    while (copy) {
        int sq_index_2 = copy.lsb(), file_2 = sq_index_2 % 8, rank_2 = sq_index_2 / 8;
        if (std::abs(file - file_2) <= 1 && rank_2 > rank && color == Color::WHITE) 
            return false; 
        if (std::abs(file - file_2) <= 1 && rank_2 < rank && color == Color::BLACK) 
            return false; 
        copy.clear(sq_index_2);
    }
    return true;  
}

inline bool promotion_threat(Board& board, Move move) {
    if (board.at(move.from()).type() != PieceType::PAWN) return false;

    Color color = board.sideToMove();
    PieceType type = board.at<Piece>(move.from()).type();
    int destination_index = move.to().index();
    int to_rank = destination_index / 8;

    Bitboard their_pawns = board.pieces(PieceType::PAWN, !color);
    bool is_passed_pawn_flag = is_passed_pawn(destination_index, color, their_pawns);

    if (is_passed_pawn_flag) {
        if ((color == Color::WHITE && to_rank >= 3) || 
            (color == Color::BLACK && to_rank <= 5)) {
            return true;
        }
    }

    return false;
}

inline bool is_mopup(Board& board) {
    int white_pawns_count = board.pieces(PieceType::PAWN, Color::WHITE).count();
    int black_pawns_count = board.pieces(PieceType::PAWN, Color::BLACK).count();

    int white_knights_count = board.pieces(PieceType::KNIGHT, Color::WHITE).count();
    int black_knights_count = board.pieces(PieceType::KNIGHT, Color::BLACK).count();

    int white_bishops_count = board.pieces(PieceType::BISHOP, Color::WHITE).count();
    int black_bishops_count = board.pieces(PieceType::BISHOP, Color::BLACK).count();

    int white_rooks_count = board.pieces(PieceType::ROOK, Color::WHITE).count();
    int black_rooks_count = board.pieces(PieceType::ROOK, Color::BLACK).count();

    int white_queens_count = board.pieces(PieceType::QUEEN, Color::WHITE).count();
    int black_queens_count = board.pieces(PieceType::QUEEN, Color::BLACK).count();

    const int white_material = white_pawns_count + white_knights_count * 3 +
                               white_bishops_count * 3 + white_rooks_count * 5 +
                               white_queens_count * 10;

    const int black_material = black_pawns_count + black_knights_count * 3 +
                               black_bishops_count * 3 + black_rooks_count * 5 +
                               black_queens_count * 10;

    if (white_pawns_count > 0 || black_pawns_count > 0) {
        // if there are pawns, it's not settled
        return false;
    } else if (std::abs(white_material - black_material) > 4) {
        // This covers cases such as KQvK, KRvK, KQvKR, KBBvK
        return true;
    }

    // Otherwise, if we have K + a minor piece, or KR vs K + minor piece, it's drawish
    return false;
}

inline int mopup_score(const Board& board) {

    int white_pawns_count = board.pieces(PieceType::PAWN, Color::WHITE).count();
    int black_pawns_count = board.pieces(PieceType::PAWN, Color::BLACK).count();

    int white_knights_count = board.pieces(PieceType::KNIGHT, Color::WHITE).count();
    int black_knights_count = board.pieces(PieceType::KNIGHT, Color::BLACK).count();

    int white_bishops_count = board.pieces(PieceType::BISHOP, Color::WHITE).count();
    int black_bishops_count = board.pieces(PieceType::BISHOP, Color::BLACK).count();

    int white_rooks_count = board.pieces(PieceType::ROOK, Color::WHITE).count();
    int black_rooks_count = board.pieces(PieceType::ROOK, Color::BLACK).count();

    int white_queens_count = board.pieces(PieceType::QUEEN, Color::WHITE).count();
    int black_queens_count = board.pieces(PieceType::QUEEN, Color::BLACK).count();

    const int white_material = white_pawns_count 
                            + white_knights_count * 3 
                            + white_bishops_count * 3 
                            + white_rooks_count * 5 
                            + white_queens_count * 10;

    const int black_material = black_pawns_count 
                            + black_knights_count * 3 
                            + black_bishops_count * 3 
                            + black_rooks_count * 5 
                            + black_queens_count * 10;


    Color winning_color = white_material > black_material ? Color::WHITE : Color::BLACK;

    Bitboard pieces = board.pieces(PieceType::PAWN, winning_color) |
                    board.pieces(PieceType::KNIGHT, winning_color) |
                    board.pieces(PieceType::BISHOP, winning_color) |
                    board.pieces(PieceType::ROOK, winning_color) |
                    board.pieces(PieceType::QUEEN, winning_color);

    Square winning_king_sq = Square(board.pieces(PieceType::KING, winning_color).lsb());
    Square losing_king_sq = Square(board.pieces(PieceType::KING, !winning_color).lsb());
    int losing_king_sq_index = losing_king_sq.index();

    int king_dist = manhattan_distance(winning_king_sq, losing_king_sq);

    int winning_material_score = winning_color == Color::WHITE ? white_material : black_material;
    int losing_material_score = winning_color == Color::WHITE ? black_material : white_material;
    int material_score = 100 * (winning_material_score - losing_material_score);

    bool bn_mate = (winning_color == Color::WHITE &&
                    white_queens_count == 0 && white_rooks_count == 0 &&
                    white_bishops_count == 1 && white_knights_count == 1) ||
                (winning_color == Color::BLACK &&
                    black_queens_count == 0 && black_rooks_count == 0 &&
                    black_bishops_count == 1 && black_knights_count == 1);

    int e4 = 28;
    int a1 = 0;
    int h1 = 7;
    int a8 = 56;
    int h8 = 63;

    if (bn_mate) {
        // Typically not needed thanks to Syzygy tablebase.
        Bitboard bishop;
        if (winning_color == Color::WHITE) {
            bishop = board.pieces(PieceType::BISHOP, Color::WHITE);
        } else {
            bishop = board.pieces(PieceType::BISHOP, Color::BLACK);
        }

        Square bishop_sq = Square(bishop.lsb());
        int bishop_rank = bishop.lsb() / 8;
        int bishop_file = bishop.lsb() % 8;

        bool dark_square_bishop = (bishop_rank + bishop_file) % 2 == 0;
        const int* bn_mate_table = dark_square_bishop ? bn_mate_dark_squares : bn_mate_light_squares;
        int score = 5000 + material_score + 150 * (14 - king_dist) + material_score + 100 * bn_mate_table[losing_king_sq_index];
        return winning_color == Color::WHITE ? score : -score;
    }

    int score = 5000 + 150 * (14 - king_dist) + material_score + 475 * manhattan_distance(losing_king_sq, Square(e4));
    return winning_color == Color::WHITE ? score : -score;

    return 0;

}

inline bool non_pawn_material(Board& board) {
    Color color = board.sideToMove();
    int non_pawn_count = board.pieces(PieceType::KNIGHT, color).count() +
                        board.pieces(PieceType::BISHOP, color).count() +
                        board.pieces(PieceType::ROOK, color).count() +
                        board.pieces(PieceType::QUEEN, color).count();
    return non_pawn_count > 0;
}

inline int piece_type_value(PieceType pt) {
    static constexpr int table[] = {
        PAWN_VALUE,     // PAWN   = 0
        KNIGHT_VALUE,   // KNIGHT = 1
        BISHOP_VALUE,   // BISHOP = 2
        ROOK_VALUE,     // ROOK   = 3
        QUEEN_VALUE,    // QUEEN  = 4
        KING_VALUE,     // KING   = 5
        0               // NONE   = 6
    };
    return table[static_cast<int>(pt)];
}

int move_score_by_table(const Board& board, Move move) {
    PieceType piece_type = board.at<Piece>(move.from()).type();
    Color color = board.at<Piece>(move.from()).color();

    int to_index = move.to().index();
    int phase = game_phase(board);

    float mid_weight = phase / 24.0f; 
    float end_weight = 1 - mid_weight;

    if (color == Color::WHITE) {
        to_index ^= 56;
    }

    if (piece_type == PieceType::PAWN) {
        return mid_weight * mid_pawn[to_index] + end_weight * end_pawn[to_index];
    } else if (piece_type == PieceType::KNIGHT) {
        return mid_weight * mid_knight[to_index] + end_weight * end_knight[to_index];
    } else if (piece_type == PieceType::BISHOP) {
        return mid_weight * mid_bishop[to_index] + end_weight * end_bishop[to_index];
    } else if (piece_type == PieceType::ROOK) {
        return mid_weight * mid_rook[to_index] + end_weight * end_rook[to_index];
    } else if (piece_type == PieceType::QUEEN) {
        return mid_weight * mid_queen[to_index] + end_weight * end_queen[to_index];
    } else if (piece_type == PieceType::KING) {
        return mid_weight * mid_king[to_index] + end_weight * end_king[to_index];
    }

    return 0;
}