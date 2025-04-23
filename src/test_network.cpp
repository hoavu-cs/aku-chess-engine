#include "nnue.hpp"
#include <iostream>



int main() {
    Network net;

    if (!loadNetwork("beans.bin", net)) {
        std::cerr << "Error loading network.\n";
        return 1;
    }

    std::cout << "Network successfully loaded from beans.bin\n";

    // Print a few values to verify
    std::cout << "First 5 bias values:\n";
    for (int i = 0; i < 5; ++i) {
        std::cout << "feature_bias[" << i << "] = " << net.feature_bias.vals[i] << "\n";
    }

    std::cout << "\nFirst 5 output weights:\n";
    for (int i = 0; i < 5; ++i) {
        std::cout << "outputWeights[" << i << "] = " << net.outputWeights[i] << "\n";
    }

    std::cout << "\nOutput bias: " << net.output_bias << "\n";

    Board board = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    Accumulator whiteAccumulator, blackAccumulator;
    makeAccumulators(board, whiteAccumulator, blackAccumulator, net);
    int eval = net.evaluate(whiteAccumulator, blackAccumulator);

    std::cout << "First 10 values of whiteAccumulator:\n";
    for (size_t i = 0; i < 10; i++)
    {
        std::cout << "whiteAccumulator[" << i << "] = " << whiteAccumulator.vals[i] << "\n";
    }

    std::cout << "\nFirst 10 values of blackAccumulator:\n";
    for (size_t i = 0; i < 10; i++)
    {
        std::cout << "blackAccumulator[" << i << "] = " << blackAccumulator.vals[i] << "\n";
    }

    std::cout << "\n Unscaled output without bias: " << eval - net.output_bias << "\n";
    

    std::cout << "Evaluation score: " << eval << "\n";

    return 0;
}
