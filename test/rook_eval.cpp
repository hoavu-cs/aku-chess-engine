#include "include/chess.hpp"
#include "evaluation_utils.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <tuple> 
#include <string>
#include <vector>
#include <algorithm>

using namespace chess;

int main() {
    Board board = Board("1r4k1/1p6/8/8/8/8/8/1RR3K1 w - - 1 1");
    std::cout << "Evaluate now" << std::endl;
    std::cout << rookValue(board, 500, Color::WHITE) << std::endl;
    std::cout << rookValue(board, 500, Color::BLACK) << std::endl;

    return 0;
}
