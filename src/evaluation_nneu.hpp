#pragma once

#include <iostream>
#include <vector>
#include <fstream>
#include <Eigen/Dense>
#include "npy.hpp"
#include "../src/chess.hpp"  // Ensure correct path

using namespace std;
using namespace Eigen;
using namespace chess;

typedef std::uint64_t U64;

// ==========================================================================
// Class for NNUE Model Evaluation
// ==========================================================================
class NNUEEvaluator {
private:
    struct NNUEModel {
        MatrixXf W1, W2, W3;
        VectorXf B1, B2, B3;
    };

    NNUEModel model;
    bool loaded = false;

    // Helper function to check if a file exists
    bool fileExists(const string& filename);

    // Load a vector from .npy file
    VectorXf loadVector(const string& filename, int size);

    // Load a matrix from .npy file
    MatrixXf loadMatrix(const string& filename, int rows, int cols);

    // Load NNUE model weights
    void loadModel();

    // ReLU activation function
    VectorXf relu(const VectorXf& x);

public:
    NNUEEvaluator();

    // Evaluate a position using NNUE
    float evaluate(const Board& board);
};

#endif // NNUE_EVALUATOR_HPP
