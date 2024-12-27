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
    Board board = Board("8/4N1k1/8/8/8/2n5/3n1N2/K7 w - - 0 1");
    std::cout << "Evaluate now" << std::endl;
    std::cout << knightValue(board, 320, Color::WHITE) << std::endl;
    std::cout << knightValue(board, 320, Color::BLACK) << std::endl;

    return 0;
}
