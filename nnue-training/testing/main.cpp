#include <iostream>
#include <fstream>
#include "chess.hpp"
#include "nnue.hpp"

using namespace chess;

bool loadWeights(
    const std::string& filename,
    std::vector<float>& hiddenWeights,   // size: 1024 * 768
    std::vector<float>& hiddenBiases,    // size: 1024
    std::vector<float>& outputWeights,   // size: 1024
    float& outputBias                    // scalar
) {
    std::ifstream fin(filename);
    if (!fin.is_open()) {
        std::cerr << "Error: Could not open weight file: " << filename << std::endl;
        return false;
    }

    hiddenWeights.resize(HIDDEN_SIZE * INPUT_SIZE);
    hiddenBiases.resize(HIDDEN_SIZE);
    outputWeights.resize(HIDDEN_SIZE);

    std::string line;
    int row = 0;

    // --- Hidden Weights (1024 rows of 768 floats) ---
    while (std::getline(fin, line) && !line.empty() && row < HIDDEN_SIZE) {
        std::istringstream ss(line);
        for (int j = 0; j < INPUT_SIZE; ++j) {
            ss >> hiddenWeights[row * INPUT_SIZE + j];
        }
        ++row;
    }

    // --- Hidden Biases (1 line of 1024 floats) ---
    while (std::getline(fin, line) && line.empty()); // skip blank line
    if (!fin || line.empty()) {
        std::cerr << "Error: Could not read hidden biases." << std::endl;
        return false;
    }
    {
        std::istringstream ss(line);
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            ss >> hiddenBiases[i];
        }
    }

    // --- Output Weights (1 line of 1024 floats) ---
    while (std::getline(fin, line) && line.empty()); // skip blank line
    if (!fin || line.empty()) {
        std::cerr << "Error: Could not read output weights." << std::endl;
        return false;
    }
    {
        std::istringstream ss(line);
        for (int i = 0; i < HIDDEN_SIZE; ++i) {
            ss >> outputWeights[i];
        }
    }

    // --- Output Bias (1 float) ---
    while (std::getline(fin, line) && line.empty()); // skip blank line
    if (!fin || line.empty()) {
        std::cerr << "Error: Could not read output bias." << std::endl;
        return false;
    }
    try {
        outputBias = std::stof(line);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing output bias: " << e.what() << std::endl;
        return false;
    }

    return true;
}




int main() {

    std::string weightFile = "nnue_weights.txt";
    std::vector<float> hiddenWeights, hiddenBiases, outputWeights;
    float outputBias = 0.0f;

    if (!loadWeights(weightFile, hiddenWeights, hiddenBiases, outputWeights, outputBias)) {
        std::cerr << "Failed to load weights." << std::endl;
        return 1;
    }

    std::string fen;
    std::cout << "Enter FEN: " << std::endl;
    std::getline(std::cin, fen);
    chess::Board board(fen);

    float evaluation = probeNNUE(board, 
        hiddenWeights.data(), 
        hiddenBiases.data(), 
        outputWeights.data(), 
        outputBias);

    std::cout << "NNUE Evaluation: " << evaluation << std::endl;


    return 0;
}

