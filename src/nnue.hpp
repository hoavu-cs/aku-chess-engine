#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

constexpr int INPUT_SIZE = 768;
constexpr int HIDDEN_SIZE = 1024;

// Clamped ReLU squared (SCReLU)
inline float screlu(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x;
}

// Sigmoid
inline float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

// Perform inference on a single input vector
float infer(
    const float* input,                         // shape: [768]
    const float* hidden_weights,               // shape: [1024][768] → flat size 1024*768
    const float* hidden_biases,                // shape: [1024]
    const float* output_weights,               // shape: [1][1024] → flat size 1024
    const float output_bias                    // scalar
) {
    float hidden[HIDDEN_SIZE];

    // Compute hidden layer with SCReLU activation
    for (int i = 0; i < HIDDEN_SIZE; ++i) {
        float sum = hidden_biases[i];

        #pragma GCC ivdep // Enable vectorization for this loop
        for (int j = 0; j < INPUT_SIZE; ++j) {
            sum += hidden_weights[i * INPUT_SIZE + j] * input[j];
        }

        hidden[i] = sum;
    }

    #pragma GCC ivdep
    for (int i = 0; i < HIDDEN_SIZE; ++i) {
        hidden[i] = screlu(hidden[i]);
    }

    // Compute output
    float output = output_bias;
    #pragma GCC ivdep 
    for (int i = 0; i < HIDDEN_SIZE; ++i) {
        output += output_weights[i] * hidden[i];
    }

    return sigmoid(output);  // final prediction
}
