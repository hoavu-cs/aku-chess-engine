
#include "evaluation_nneu.hpp"

// ==========================================================================
// Helper function to check if a file exists
// ==========================================================================
bool NNUEEvaluator::fileExists(const string& filename) {
    ifstream file(filename);
    return file.good();
}

// ==========================================================================
// Load a vector from .npy file
// ==========================================================================
VectorXf NNUEEvaluator::loadVector(const string& filename, int size) {
    if (!fileExists(filename)) {
        cerr << "Error: Missing weight file - " << filename << endl;
        exit(EXIT_FAILURE);
    }

    npy::npy_data d = npy::read_npy<float>(filename);
    vector<float> data = d.data;

    if (data.size() != size) {
        cerr << "Error: Expected " << size << " elements in " << filename
             << ", but got " << data.size() << endl;
        exit(EXIT_FAILURE);
    }

    return Map<VectorXf>(data.data(), size);
}

// ==========================================================================
// Load a matrix from .npy file
// ==========================================================================
MatrixXf NNUEEvaluator::loadMatrix(const string& filename, int rows, int cols) {
    if (!fileExists(filename)) {
        cerr << "Error: Missing weight file - " << filename << endl;
        exit(EXIT_FAILURE);
    }

    npy::npy_data d = npy::read_npy<float>(filename);
    vector<float> data = d.data;

    // Load as row-major first, then convert explicitly to column-major
    Matrix<float, Dynamic, Dynamic, RowMajor> rowMajorMatrix =
        Map<Matrix<float, Dynamic, Dynamic, RowMajor>>(data.data(), rows, cols);

    MatrixXf columnMajorMatrix = rowMajorMatrix;
    return columnMajorMatrix;
}

// ==========================================================================
// Load the NNUE model (Once)
// ==========================================================================
void NNUEEvaluator::loadModel() {
    if (loaded) return; // Avoid reloading

    cout << "Loading NNUE weights..." << endl;
    model.W1 = loadMatrix("fc1.weight.npy", 256, 768);
    model.B1 = loadVector("fc1.bias.npy", 256);
    
    model.W2 = loadMatrix("fc2.weight.npy", 128, 256);
    model.B2 = loadVector("fc2.bias.npy", 128);
    
    model.W3 = loadMatrix("fc3.weight.npy", 1, 128);
    model.B3 = loadVector("fc3.bias.npy", 1);

    loaded = true;
    cout << "Weights loaded successfully!" << endl;
}

// ==========================================================================
// ReLU activation function
// ==========================================================================
VectorXf NNUEEvaluator::relu(const VectorXf& x) {
    return x.cwiseMax(0.0f);  // ReLU: max(0, x)
}

// ==========================================================================
// Constructor: Load model once
// ==========================================================================
NNUEEvaluator::NNUEEvaluator() {
    loadModel();
}

// ==========================================================================
// Evaluate a chess position using NNUE
// ==========================================================================
float NNUEEvaluator::evaluate(const Board& board) {
    std::vector<U64> bitboards = {
        board.pieces(PieceType::PAWN, Color::BLACK).getBits(),  // Black first (0-5)
        board.pieces(PieceType::KNIGHT, Color::BLACK).getBits(),
        board.pieces(PieceType::BISHOP, Color::BLACK).getBits(),
        board.pieces(PieceType::ROOK, Color::BLACK).getBits(),
        board.pieces(PieceType::QUEEN, Color::BLACK).getBits(),
        board.pieces(PieceType::KING, Color::BLACK).getBits(),
        board.pieces(PieceType::PAWN, Color::WHITE).getBits(),  // White second (6-11)
        board.pieces(PieceType::KNIGHT, Color::WHITE).getBits(),
        board.pieces(PieceType::BISHOP, Color::WHITE).getBits(),
        board.pieces(PieceType::ROOK, Color::WHITE).getBits(),
        board.pieces(PieceType::QUEEN, Color::WHITE).getBits(),
        board.pieces(PieceType::KING, Color::WHITE).getBits()
    };

    VectorXf input(768);
    input.setZero();

    for (int piece = 0; piece < 12; ++piece) {
        U64 bb = bitboards[piece];
        for (int square = 0; square < 64; ++square) {
            if (bb & (1ULL << square)) {  
                input(piece * 64 + square) = 1.0f;
            }
        }
    }

    VectorXf x1 = relu(model.W1 * input + model.B1);
    VectorXf x2 = relu(model.W2 * x1 + model.B2);
    float output = (model.W3 * x2 + model.B3)(0);

    return output * 10000;  // Scale output to match evaluation format
}
