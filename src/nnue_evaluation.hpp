#ifndef NNUE_EVALUATOR_HPP
#define NNUE_EVALUATOR_HPP

#include <iostream>
#include <vector>
#include <bitset>
#include <string>
#include <fstream>
#include <Eigen/Dense>
#include "npy.hpp"
#include "../src/chess.hpp"  // Ensure correct path

using namespace std;
using namespace Eigen;
using namespace chess;

typedef std::uint64_t U64;  // 64-bit unsigned integer for bitboards

/* ==========================================================================
   NNUE Evaluator Class
========================================================================== */
class NNUEEvaluator {
private:
    struct NNUEModel {
        Matrix<int8_t, Dynamic, Dynamic> W1, W2, W3;  // Use 8-bit integers
        Vector<int16_t, Dynamic> B1, B2, B3;  // Use 16-bit for biases
    };

    NNUEModel model;
    bool loaded = false;

    /* --------------------------------------------------------------
       Helper function: Check if a file exists
    -------------------------------------------------------------- */
    bool isGood(const string& filename) const {
        ifstream file(filename);
        return file.good();
    }

    /* --------------------------------------------------------------
       Load a vector from a .npy file (Convert float → int16_t)
    -------------------------------------------------------------- */
    Vector<int16_t, Dynamic> loadVector(const string& filename, int size) {
        if (!isGood(filename)) {
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

        // Convert float → int16_t (fixed-point representation)
        Vector<int16_t, Dynamic> result(size);
        for (int i = 0; i < size; ++i) {
            result(i) = static_cast<int16_t>(data[i] * 256);  // Scale to int16
        }

        return result;
    }

    /* --------------------------------------------------------------
       Load a matrix from a .npy file (Convert float → int8_t)
    -------------------------------------------------------------- */
    Matrix<int8_t, Dynamic, Dynamic> loadMatrix(const string& filename, int rows, int cols) {
        if (!isGood(filename)) {
            cerr << "Error: Missing weight file - " << filename << endl;
            exit(EXIT_FAILURE);
        }

        npy::npy_data d = npy::read_npy<float>(filename);
        vector<float> data = d.data;

        // Load as row-major first, then convert explicitly to int8_t
        Matrix<int8_t, Dynamic, Dynamic> result(rows, cols);
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                result(i, j) = static_cast<int8_t>(data[i * cols + j] * 128);  // Scale to int8
            }
        }
        return result;
    }

    /* --------------------------------------------------------------
       Load the NNUE model (Once)
    -------------------------------------------------------------- */
    void loadModel() {
        if (loaded) return; // Avoid reloading

        cout << "Loading NNUE weights (Optimized)..." << endl;
        model.W1 = loadMatrix("nneu/fc1.weight.npy", 256, 768);
        model.B1 = loadVector("nneu/fc1.bias.npy", 256);
        
        model.W2 = loadMatrix("nneu/fc2.weight.npy", 128, 256);
        model.B2 = loadVector("nneu/fc2.bias.npy", 128);
        
        model.W3 = loadMatrix("nneu/fc3.weight.npy", 1, 128);
        model.B3 = loadVector("nneu/fc3.bias.npy", 1);

        loaded = true;
        cout << "Optimized Weights Loaded Successfully!" << endl;
    }

    /* --------------------------------------------------------------
       ReLU Activation Function (Vectorized)
    -------------------------------------------------------------- */
    Vector<int16_t, Dynamic> relu(const Vector<int16_t, Dynamic>& x) const {
        return x.cwiseMax(0);  // Integer-based ReLU
    }

public:
    /* --------------------------------------------------------------
       Constructor: Load model once
    -------------------------------------------------------------- */
    NNUEEvaluator() {
        loadModel();
    }

    /* --------------------------------------------------------------
       Convert bitboards (12x uint64_t) into a 768-element feature vector
    -------------------------------------------------------------- */
    Vector<int8_t, Dynamic> bitboardsToInput(const vector<U64>& bitboards) const {
        Vector<int8_t, Dynamic> input(768);
        input.setZero();

        for (int piece = 0; piece < 12; ++piece) {
            U64 bb = bitboards[piece];
            for (int square = 0; square < 64; ++square) {
                if (bb & (1ULL << square)) {  
                    input(piece * 64 + square) = 1;  // Use int8_t (binary)
                }
            }
        }
        return input;
    }

    /* --------------------------------------------------------------
       Evaluate a chess position using Optimized NNUE (Fixed-Point)
    -------------------------------------------------------------- */
    float evaluate(const Board& board) const {
        vector<U64> bitboards = {
            board.pieces(PieceType::PAWN, Color::BLACK).getBits(),
            board.pieces(PieceType::KNIGHT, Color::BLACK).getBits(),
            board.pieces(PieceType::BISHOP, Color::BLACK).getBits(),
            board.pieces(PieceType::ROOK, Color::BLACK).getBits(),
            board.pieces(PieceType::QUEEN, Color::BLACK).getBits(),
            board.pieces(PieceType::KING, Color::BLACK).getBits(),
            board.pieces(PieceType::PAWN, Color::WHITE).getBits(),
            board.pieces(PieceType::KNIGHT, Color::WHITE).getBits(),
            board.pieces(PieceType::BISHOP, Color::WHITE).getBits(),
            board.pieces(PieceType::ROOK, Color::WHITE).getBits(),
            board.pieces(PieceType::QUEEN, Color::WHITE).getBits(),
            board.pieces(PieceType::KING, Color::WHITE).getBits()
        };

        // Convert bitboards to NNUE input
        Vector<int8_t, Dynamic> input = bitboardsToInput(bitboards);

        // Optimized Fixed-Point NNUE Inference
        Vector<int16_t, Dynamic> x1 = relu(model.W1.cast<int16_t>() * input.cast<int16_t>() + model.B1);
        Vector<int16_t, Dynamic> x2 = relu(model.W2.cast<int16_t>() * x1 + model.B2);
        int16_t output = (model.W3.cast<int16_t>() * x2 + model.B3)(0);

        return output / 256.0f;  // Scale back to float evaluation
    }
};

#endif  // NNUE_EVALUATOR_HPP
