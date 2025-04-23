#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>

constexpr int INPUT_SIZE = 768;
constexpr int HIDDEN_SIZE = 512;
constexpr int SCALE = 400;
constexpr int QA = 255;
constexpr int QB = 64;

/*--------------------------------------------------------------------------------------------
    Clip ReLU
--------------------------------------------------------------------------------------------*/
inline int crelu(int16_t x) {
    int val = static_cast<int>(x);
    return std::clamp(val, 0, static_cast<int>(QA));
}

/*--------------------------------------------------------------------------------------------
    Forward declaration
--------------------------------------------------------------------------------------------*/
struct Network;

/*--------------------------------------------------------------------------------------------
    Accumulator
--------------------------------------------------------------------------------------------*/
struct alignas(64) Accumulator {
    std::array<int16_t, HIDDEN_SIZE> vals;

    // Initialize from network's bias
    static Accumulator from_bias(const Network& net) {
        return net.feature_bias;
    }

    // Add feature vector to accumulator
    void add_feature(size_t feature_idx, const Network& net) {
        for (size_t i = 0; i < HIDDEN_SIZE; ++i) {
            vals[i] += net.feature_weights[feature_idx].vals[i];
        }
    }

    // Remove feature vector from accumulator
    void remove_feature(size_t feature_idx, const Network& net) {
        for (size_t i = 0; i < HIDDEN_SIZE; ++i) {
            vals[i] -= net.feature_weights[feature_idx].vals[i];
        }
    }
};

/*--------------------------------------------------------------------------------------------
    768 -> HIDDEN_SIZE x 2 -> 1
    Network architecture.
--------------------------------------------------------------------------------------------*/
struct Network {
    std::array<Accumulator, 768> feature_weights;
    Accumulator feature_bias;
    std::array<int16_t, 2 * HIDDEN_SIZE> output_weights;
    int16_t output_bias;

    int evaluate(const Accumulator& us, const Accumulator& them) const {
        int output = static_cast<int>(output_bias);

        // Side-to-move accumulator
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            output += crelu(us.vals[i]) * static_cast<int>(output_weights[i]);
        }

        // Not-side-to-move accumulator
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            output += crelu(them.vals[i]) * static_cast<int>(output_weights[HIDDEN_SIZE + i]);
        }

        output *= SCALE;
        output /= static_cast<int>(QA) * static_cast<int>(QB); // Remove quantization

        return output;
    }
};
