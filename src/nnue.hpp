#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <array>
#include <fstream>
#include <cstdint>
#include <string>
#include <iostream>

constexpr int INPUT_SIZE = 768;
constexpr int HIDDEN_SIZE = 64;
constexpr int SCALE = 400;
constexpr int QA = 255;
constexpr int QB = 64;

/*--------------------------------------------------------------------------------------------
    Calculate index of a piece, square, and side to move
    Square: 0 - 63
    PieceType: Pawn = 0, Knight = 1, Bishop = 2, Rook = 3, Queen = 4, King = 5
    Side: White = 0, Black = 1
--------------------------------------------------------------------------------------------*/
inline int calculateIndex(int side, int pieceType, int square) {
    return side * 64 * 6 + pieceType * 64 + square;
}

/*--------------------------------------------------------------------------------------------
    Clip ReLU
--------------------------------------------------------------------------------------------*/
inline int crelu(int16_t x) {
    int val = static_cast<int>(x);
    return std::clamp(val, 0, static_cast<int>(QA));
}

/*--------------------------------------------------------------------------------------------
    Square Clip ReLU (SCReLU)
--------------------------------------------------------------------------------------------*/
inline int screlu(int16_t x) {
    int val = std::clamp(static_cast<int>(x), 0, static_cast<int>(QA));
    return val * val;
}

/*--------------------------------------------------------------------------------------------
    Forward declaration
--------------------------------------------------------------------------------------------*/
struct Network;

/*--------------------------------------------------------------------------------------------
    Accumulator
--------------------------------------------------------------------------------------------*/
struct Accumulator {
    std::array<int16_t, HIDDEN_SIZE> vals;

    static Accumulator fromBias(const Network& net);
    void addFeature(size_t feature_idx, const Network& net);
    void removeFeature(size_t feature_idx, const Network& net);
};


/*--------------------------------------------------------------------------------------------
    768 -> HIDDEN_SIZE x 2 -> 1
    Network architecture:
    x1 : 768 for side-to-move
    x2 : 768 for not-side-to-move

    h1 = Wx1 + b  
    h2 = Wx2 + b

    o = O1 * relu(h1) + O2 * relu(h2) + c
--------------------------------------------------------------------------------------------*/
struct Network {
    std::array<Accumulator, 768> featureWeights;
    Accumulator feature_bias;
    std::array<int16_t, 2 * HIDDEN_SIZE> outputWeights;
    int16_t output_bias;

    int evaluate(const Accumulator& us, const Accumulator& them) const {
        int output = static_cast<int>(output_bias);

        // Side-to-move accumulator
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            output += screlu(us.vals[i]) * static_cast<int>(outputWeights[i]);
        }

        // Not-side-to-move accumulator
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            output += screlu(them.vals[i]) * static_cast<int>(outputWeights[HIDDEN_SIZE + i]);
        }

        output *= SCALE;
        output /= static_cast<int>(QA) * static_cast<int>(QB); // Remove quantization

        return output;
    }
};


/*--------------------------------------------------------------------------------------------
    Accumulator functions.
--------------------------------------------------------------------------------------------*/
inline Accumulator Accumulator::fromBias(const Network& net) {
    return net.feature_bias;
}

inline void Accumulator::addFeature(size_t feature_idx, const Network& net) {
    for (size_t i = 0; i < HIDDEN_SIZE; ++i) {
        vals[i] += net.featureWeights[feature_idx].vals[i];
    }
}

inline void Accumulator::removeFeature(size_t feature_idx, const Network& net) {
    for (size_t i = 0; i < HIDDEN_SIZE; ++i) {
        vals[i] -= net.featureWeights[feature_idx].vals[i];
    }
}

/*--------------------------------------------------------------------------------------------
    Load network from file.
--------------------------------------------------------------------------------------------*/
bool loadNetwork(const std::string& filepath, Network& net) {
    std::ifstream stream(filepath, std::ios::binary);
    if (!stream.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return false;
    }

    // Load feature weights: 768 x 64 = 393216 int16_t
    for (int i = 0; i < INPUT_SIZE; ++i) {
        stream.read(reinterpret_cast<char*>(net.featureWeights[i].vals.data()),
                    HIDDEN_SIZE * sizeof(int16_t));
    }

    // Load hidden layer bias: 512 int16_t
    stream.read(reinterpret_cast<char*>(net.feature_bias.vals.data()),
                HIDDEN_SIZE * sizeof(int16_t));

    // Load output weights: 2 x 512 int16_t
    stream.read(reinterpret_cast<char*>(net.outputWeights.data()),
                2 * HIDDEN_SIZE * sizeof(int16_t));

    // Load output bias: 1 int16_t
    stream.read(reinterpret_cast<char*>(&net.output_bias), sizeof(int16_t));

    if (!stream) {
        std::cerr << "Failed to read full network from file.\n";
        return false;
    }

    return true;
}

/*--------------------------------------------------------------------------------------------
    Create accumulators for white and black pieces.
--------------------------------------------------------------------------------------------*/
inline int mirror_square(int sq) {
    return (sq ^ 56); // flips rank (A1 to A8, H2 to H7, etc.)
}

void createAccumulator(Board& board, Accumulator& whiteAccumulator, Accumulator& blackAccumulator) {
    // Initialize the accumulators
    whiteAccumulator = Accumulator::fromBias(evalNetwork);
    blackAccumulator = Accumulator::fromBias(evalNetwork);

    // Add features for each piece on the board
    Bitboard whitePawns = board.pieces(PieceType::PAWN, Color::WHITE);
    Bitboard whiteKnights = board.pieces(PieceType::KNIGHT, Color::WHITE);
    Bitboard whiteBishops = board.pieces(PieceType::BISHOP, Color::WHITE);
    Bitboard whiteRooks = board.pieces(PieceType::ROOK, Color::WHITE);
    Bitboard whiteQueens = board.pieces(PieceType::QUEEN, Color::WHITE);
    Bitboard whiteKings = board.pieces(PieceType::KING, Color::WHITE);

    Bitboard blackPawns = board.pieces(PieceType::PAWN, Color::BLACK);
    Bitboard blackKnights = board.pieces(PieceType::KNIGHT, Color::BLACK);
    Bitboard blackBishops = board.pieces(PieceType::BISHOP, Color::BLACK);
    Bitboard blackRooks = board.pieces(PieceType::ROOK, Color::BLACK);
    Bitboard blackQueens = board.pieces(PieceType::QUEEN, Color::BLACK);
    Bitboard blackKings = board.pieces(PieceType::KING, Color::BLACK);

    for (int i = 0; i < 12; i++) {
        Bitboard bb;
        if (i == 0) bb = whitePawns;
        else if (i == 1) bb = whiteKnights;
        else if (i == 2) bb = whiteBishops;
        else if (i == 3) bb = whiteRooks;
        else if (i == 4) bb = whiteQueens;
        else if (i == 5) bb = whiteKings;
        else if (i == 6) bb = blackPawns;
        else if (i == 7) bb = blackKnights;
        else if (i == 8) bb = blackBishops;
        else if (i == 9) bb = blackRooks;
        else if (i == 10) bb = blackQueens;
        else if (i == 11) bb = blackKings;

        while (bb) {
            int sq = bb.lsb();
            bb.clear(sq);
        
            int type = i % 6;
            int color = (i < 6) ? 0 : 1;
        
            // White
            if (color == 0)
                whiteAccumulator.addFeature(calculateIndex(0, type, sq), evalNetwork);
            else
                blackAccumulator.addFeature(calculateIndex(1, type, sq), evalNetwork);
        
            // Black
            int msq = mirror_square(sq);
            if (color == 1)
                blackAccumulator.addFeature(calculateIndex(0, type, msq), evalNetwork);
            else
                whiteAccumulator.addFeature(calculateIndex(1, type, msq), evalNetwork);
        }
        
    }
}