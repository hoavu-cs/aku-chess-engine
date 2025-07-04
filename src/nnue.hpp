#pragma once

#include <vector>
#include <array>
#include <fstream>
#include <cstdint>
#include <iostream>
#include "chess.hpp"

using namespace chess;

constexpr int INPUT_SIZE = 768;
constexpr int HIDDEN_SIZE = 1024;
constexpr int SCALE = 400;
constexpr int QA = 255;
constexpr int QB = 64;

// Function Declarations
inline int calculate_index(int side, int pieceType, int square);
inline int piecetype_to_idx(PieceType type);
inline int crelu(int16_t x);
inline int screlu(int16_t x);
inline int mirror_sq(int sq);
struct Accumulator;
struct Network;
bool load_network(const std::string& filepath, Network& net);
void make_accumulators(Board& board, Accumulator& white_accumulator, Accumulator& black_accumulator, Network& eval_network);
void add_accumulators(Board& board, Move& move, Accumulator& white_accumulator, Accumulator& black_accumulator, Network& eval_network);
void subtract_accumulators(Board& board, Move& move, Accumulator& white_accumulator, Accumulator& black_accumulator, Network& eval_network);

// Function Definitions

// Calculate index of a piece, square, and side to move
// Square: 0 - 63
// PieceType: Pawn = 0, Knight = 1, Bishop = 2, Rook = 3, Queen = 4, King = 5
// Side: us = 0, them = 1
inline int calculate_index(int side, int pieceType, int square) {
    return side * 64 * 6 + pieceType * 64 + square;
}

// PieceType to piece index
inline int piecetype_to_idx(PieceType type) {
    if (type == PieceType::PAWN) return 0;
    if (type == PieceType::KNIGHT) return 1;
    if (type == PieceType::BISHOP) return 2;
    if (type == PieceType::ROOK) return 3;
    if (type == PieceType::QUEEN) return 4;
    if (type == PieceType::KING) return 5;
    return -1; // Invalid piece type
}

// Clip ReLU
inline int crelu(int16_t x) {
    int val = static_cast<int>(x);
    return std::clamp(val, 0, static_cast<int>(QA));
}

// Square Clip ReLU (SCReLU)
inline int screlu(int16_t x) {
    int val = std::clamp(static_cast<int>(x), 0, static_cast<int>(QA));
    return val * val;
}

// Accumulator
struct Accumulator {
    alignas(32) std::array<int16_t, HIDDEN_SIZE> vals;
    static Accumulator from_bias(const Network& net);
    void add_feature(size_t feature_idx, const Network& net);
    void remove_feature(size_t feature_idx, const Network& net);
};

// (768 -> HIDDEN_SIZE) x 2 -> 1
// Network architecture:
// x1 : 768 for side-to-move
// x2 : 768 for not-side-to-move
// h1 = Wx1 + b  
// h2 = Wx2 + b
// o = O1 * relu(h1) + O2 * relu(h2) + c
struct Network {
    alignas(32) std::array<Accumulator, 768> feature_weights;
    Accumulator feature_bias;
    std::array<int16_t, 2 * HIDDEN_SIZE> output_weights;
    int16_t output_bias;

    int evaluate(const Accumulator& us, const Accumulator& them) const {
        int output = 0;
    
        #pragma omp simd reduction(+:output)
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            output += screlu(us.vals[i]) * static_cast<int>(output_weights[i]) +
                      screlu(them.vals[i]) * static_cast<int>(output_weights[HIDDEN_SIZE + i]);
        }
    
        output /= QA;
        output += static_cast<int>(output_bias);

        output *= SCALE;
        output /= QA * QB;
    
        return output;
    }
};

// Accumulator functions
inline Accumulator Accumulator::from_bias(const Network& net) {
    return net.feature_bias;
}

inline void Accumulator::add_feature(size_t feature_idx, const Network& net) {
    #pragma omp simd
    for (size_t i = 0; i < HIDDEN_SIZE; ++i) {
        vals[i] += net.feature_weights[feature_idx].vals[i];
    }
}

inline void Accumulator::remove_feature(size_t feature_idx, const Network& net) {
    #pragma omp simd
    for (size_t i = 0; i < HIDDEN_SIZE; ++i) {
        vals[i] -= net.feature_weights[feature_idx].vals[i];
    }
}

// Load network from file
bool load_network(const std::string& filepath, Network& net) {
    std::ifstream stream(filepath, std::ios::binary);
    if (!stream.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return false;
    }

    // Layer 1
    for (int i = 0; i < INPUT_SIZE; ++i) {
        stream.read(reinterpret_cast<char*>(net.feature_weights[i].vals.data()),
                    HIDDEN_SIZE * sizeof(int16_t));
    }

    // Load hidden layer bias: HL int16_t
    stream.read(reinterpret_cast<char*>(net.feature_bias.vals.data()),
                HIDDEN_SIZE * sizeof(int16_t));

    // Load output weights: 2 x HL int16_t
    stream.read(reinterpret_cast<char*>(net.output_weights.data()), 2 * HIDDEN_SIZE * sizeof(int16_t));

    // Load output bias: 1 int16_t
    stream.read(reinterpret_cast<char*>(&net.output_bias), sizeof(int16_t));

    if (!stream) {
        std::cerr << "Failed to read full network from file.\n";
        return false;
    }

    return true;
}


// Create accumulators for white and black pieces.
inline int mirror_sq(int sq) {
    return (sq ^ 56); // flips rank (A1 to A8, H2 to H7, etc.)
}

void make_accumulators(Board& board, Accumulator& white_accumulator, Accumulator& black_accumulator, Network& eval_network) {
    // Initialize the accumulators
    white_accumulator = Accumulator::from_bias(eval_network);
    black_accumulator = Accumulator::from_bias(eval_network);

    Bitboard bitboards[12] = {
        board.pieces(PieceType::PAWN, Color::WHITE),
        board.pieces(PieceType::KNIGHT, Color::WHITE),
        board.pieces(PieceType::BISHOP, Color::WHITE),
        board.pieces(PieceType::ROOK, Color::WHITE),
        board.pieces(PieceType::QUEEN, Color::WHITE),
        board.pieces(PieceType::KING, Color::WHITE),

        board.pieces(PieceType::PAWN, Color::BLACK),
        board.pieces(PieceType::KNIGHT, Color::BLACK),
        board.pieces(PieceType::BISHOP, Color::BLACK),
        board.pieces(PieceType::ROOK, Color::BLACK),
        board.pieces(PieceType::QUEEN, Color::BLACK),
        board.pieces(PieceType::KING, Color::BLACK)
    };

    for (int i = 0; i < 12; i++) {
        Bitboard bb = bitboards[i];

        while (bb) {
            int sq = bb.lsb();
            int msq = mirror_sq(sq);
            bb.clear(sq);
            int type = i % 6;
            bool white = (i < 6);
        
            if (white) {
                // from White’s view
                white_accumulator.add_feature(calculate_index(0, type, sq),  eval_network); // us
                black_accumulator.add_feature(calculate_index(1, type, msq), eval_network); // them
            } else {
                // from Black’s view
                black_accumulator.add_feature(calculate_index(0, type, msq),  eval_network); // us
                white_accumulator.add_feature(calculate_index(1, type, sq), eval_network); // them
            }
        }
        
    }
}

// Update accumulator given a move.
// Work in progress: need to consider castling &  enpassant.
// To be called before board.makeMove(move).
void add_accumulators(Board& board, 
                        Move& move, 
                        Accumulator& white_accumulator, 
                        Accumulator& black_accumulator,
                        Network& eval_network) {

    Color color = board.sideToMove();
    PieceType piece_type = board.at<Piece>(move.from()).type();

    int piece_idx = piecetype_to_idx(piece_type);
    bool is_promotion = move.typeOf() & Move::PROMOTION;
    bool is_enpassant = move.typeOf() & Move::ENPASSANT;
    bool is_castling = move.typeOf() & Move::CASTLING; 
    bool is_null_move = move.typeOf() & Move::NULL_MOVE;

    if (is_null_move) {
        return; 
    }

    if (is_promotion || is_enpassant || is_castling) {
        // For now calculate from scratch for promotion and enpassant
        board.makeMove(move);
        make_accumulators(board, white_accumulator, black_accumulator, eval_network);
        board.unmakeMove(move);
        return;
    }

    if (color == Color::WHITE) {
        int from_idx_us = calculate_index(0, piece_idx, move.from().index());
        int to_idx_us = calculate_index(0, piece_idx, move.to().index());
        int from_idx_them = calculate_index(1, piece_idx, mirror_sq(move.from().index()));
        int to_idx_them = calculate_index(1, piece_idx, mirror_sq(move.to().index()));

        // Calculate index of from and to from "us" perspective
        white_accumulator.remove_feature(from_idx_us, eval_network);
        white_accumulator.add_feature(to_idx_us, eval_network);

        black_accumulator.remove_feature(from_idx_them, eval_network);
        black_accumulator.add_feature(to_idx_them, eval_network);

        if (board.isCapture(move)) {
            // Remove the captured piece
            PieceType captured = board.at<Piece>(move.to()).type();
            int capture_idx = piecetype_to_idx(captured);

            // remove the black piece from white's perspective
            int capture_index_us = calculate_index(1, capture_idx, move.to().index());
            white_accumulator.remove_feature(capture_index_us, eval_network);

            // remove the black piece from black's perspective
            int capture_index_them = calculate_index(0, capture_idx, mirror_sq(move.to().index()));
            black_accumulator.remove_feature(capture_index_them, eval_network);
        }

    } else {
        int from_idx_us = calculate_index(0, piece_idx, mirror_sq(move.from().index()));
        int to_idx_us = calculate_index(0, piece_idx, mirror_sq(move.to().index()));
        int from_idx_them = calculate_index(1, piece_idx, move.from().index());
        int to_idx_them = calculate_index(1, piece_idx, move.to().index());

        black_accumulator.remove_feature(from_idx_us, eval_network);
        black_accumulator.add_feature(to_idx_us, eval_network);

        white_accumulator.remove_feature(from_idx_them, eval_network);
        white_accumulator.add_feature(to_idx_them, eval_network);

        if (board.isCapture(move)) {
            PieceType captured = board.at<Piece>(move.to()).type();
            int capture_idx = piecetype_to_idx(captured);

            // remove the white piece from black's perspective
            int capture_index_us = calculate_index(1, capture_idx, mirror_sq(move.to().index()));
            black_accumulator.remove_feature(capture_index_us, eval_network);

            // remove the white piece from white's perspective
            int capture_index_them = calculate_index(0, capture_idx, move.to().index());
            white_accumulator.remove_feature(capture_index_them, eval_network);
        }
    }
}

// Update accumulator given an unmakeMove.
// To be called before board.unmakeMove(move).
void subtract_accumulators(Board& board, 
                                Move& move, 
                                Accumulator& white_accumulator, 
                                Accumulator& black_accumulator,
                                Network& eval_network) {

    bool is_promotion = move.typeOf() & Move::PROMOTION;
    bool is_enpassant = move.typeOf() & Move::ENPASSANT;
    bool is_castle = move.typeOf() & Move::CASTLING;      
    bool is_null_move = move.typeOf() & Move::NULL_MOVE;

    board.unmakeMove(move);
    make_accumulators(board, white_accumulator, black_accumulator, eval_network);
    board.makeMove(move);
    return;

    if (is_null_move) {
        return; 
    }

    if (is_promotion || is_enpassant || is_castle) {
        // For now calculate from scratch for promotion and enpassant
        board.unmakeMove(move);
        make_accumulators(board, white_accumulator, black_accumulator, eval_network);
        board.makeMove(move);
        return;
    }

    board.unmakeMove(move); 

    Color color = board.sideToMove(); 
    PieceType pieceType = board.at<Piece>(move.from()).type();
    int piece_idx = piecetype_to_idx(pieceType);

    if (color == Color::WHITE) {
        int from_idx_us = calculate_index(0, piece_idx, move.from().index());
        int to_idx_us = calculate_index(0, piece_idx, move.to().index());
        int from_idx_them = calculate_index(1, piece_idx, mirror_sq(move.from().index()));
        int to_idx_them = calculate_index(1, piece_idx, mirror_sq(move.to().index()));

        // Calculate index of from and to from "us" perspective
        white_accumulator.add_feature(from_idx_us, eval_network);
        white_accumulator.remove_feature(to_idx_us, eval_network);
        black_accumulator.add_feature(from_idx_them, eval_network);
        black_accumulator.remove_feature(to_idx_them, eval_network);


        if (board.isCapture(move)) {
            PieceType captured = board.at<Piece>(move.to()).type();
            int captured_idx = piecetype_to_idx(captured);

            // Put the black piece back in its original position from white's perspective
            int capture_idx_us = calculate_index(1, captured_idx, move.to().index());
            white_accumulator.add_feature(capture_idx_us, eval_network);
            // Put the black piece back in its original position from black's perspective
            int capture_idx_them = calculate_index(0, captured_idx, mirror_sq(move.to().index()));
            black_accumulator.add_feature(capture_idx_them, eval_network); 
        }
    } else {
        int from_idx_us = calculate_index(0, piece_idx, mirror_sq(move.from().index()));
        int to_idx_us = calculate_index(0, piece_idx, mirror_sq(move.to().index()));
        int from_idx_them = calculate_index(1, piece_idx, move.from().index());
        int to_idx_them = calculate_index(1, piece_idx, move.to().index());

        black_accumulator.remove_feature(from_idx_us, eval_network);
        black_accumulator.add_feature(to_idx_us, eval_network);
        white_accumulator.remove_feature(from_idx_them, eval_network);
        white_accumulator.add_feature(to_idx_them, eval_network);

        if (board.isCapture(move)) {            
            PieceType captured = board.at<Piece>(move.to()).type();
            int captured_idx = piecetype_to_idx(captured);

            // Put the white piece back from black's perspective
            int capture_idx_us = calculate_index(1, captured_idx, mirror_sq(move.to().index()));
            black_accumulator.add_feature(capture_idx_us, eval_network);
            // Put the white piece from white's perspective
            int capture_idx_them = calculate_index(0, captured_idx, move.to().index());
            white_accumulator.add_feature(capture_idx_them, eval_network);
        }
    }

    board.makeMove(move);
}
