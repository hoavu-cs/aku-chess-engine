#include "../src/chess.hpp"
#include "../src/evaluation.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <tuple> 
#include <string>
#include <vector>
#include <algorithm>

using namespace chess;

int main() {
    Board board = Board("8/1n4k1/8/1N1Bp1N1/1pPb2pP/5n2/8/1K6 w - - 0 1");
    std::cout << "Evaluate now" << std::endl;
    evaluate(board);

    return 0;
}
