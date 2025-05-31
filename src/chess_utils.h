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
const int KING_VALUE = 0;

const int bn_mate_light_squares[64] = {
    0, 10, 20, 30, 40, 50, 60, 70,
    10, 20, 30, 40, 50, 60, 70, 60,
    20, 30, 40, 50, 60, 70, 60, 50,
    30, 40, 50, 60, 70, 60, 50, 40,
    40, 50, 60, 70, 60, 50, 40, 30,
    50, 60, 70, 60, 50, 40, 30, 20,
    60, 70, 60, 50, 40, 30, 20, 10,
    70, 60, 50, 40, 30, 20, 10, 0
};

const int bn_mate_dark_squares[64] = {
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
    if (pt == PieceType::PAWN)   return PAWN_VALUE;
    if (pt == PieceType::KNIGHT) return KNIGHT_VALUE;
    if (pt == PieceType::BISHOP) return BISHOP_VALUE;
    if (pt == PieceType::ROOK)   return ROOK_VALUE;
    if (pt == PieceType::QUEEN)  return QUEEN_VALUE;
    if (pt == PieceType::KING)   return KING_VALUE;
    return 0;
}
