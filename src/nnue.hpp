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
    PieceType: Pawn = 0, Knight = 1, Bishop, Rook, Queen, King
    Side: White = 0, Black = 1
--------------------------------------------------------------------------------------------*/
inline int calculate_index(int side, int pieceType, int square) {
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

    static Accumulator from_bias(const Network& net);
    void add_feature(size_t feature_idx, const Network& net);
    void remove_feature(size_t feature_idx, const Network& net);
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
    Accumulator from bias
--------------------------------------------------------------------------------------------*/
inline Accumulator Accumulator::from_bias(const Network& net) {
    return net.feature_bias;
}

inline void Accumulator::add_feature(size_t feature_idx, const Network& net) {
    for (size_t i = 0; i < HIDDEN_SIZE; ++i) {
        vals[i] += net.featureWeights[feature_idx].vals[i];
    }
}

inline void Accumulator::remove_feature(size_t feature_idx, const Network& net) {
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