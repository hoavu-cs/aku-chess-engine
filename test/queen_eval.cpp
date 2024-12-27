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
    Board board = Board("8/6k1/4Q3/8/1q6/8/2Q5/K7 w - - 0 1");
    std::cout << "Evaluate now" << std::endl;
    std::cout << queenValue(board, 900, Color::WHITE) << std::endl;
    std::cout << queenValue(board, 900, Color::BLACK) << std::endl;

    return 0;
}
