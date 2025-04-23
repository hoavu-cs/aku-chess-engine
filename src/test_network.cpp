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

    return 0;
}
