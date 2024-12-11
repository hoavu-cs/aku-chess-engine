#include "../src/chess.hpp"
#include "../src/evaluation_utils.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <tuple> 
#include <string>
#include <vector>
#include <algorithm>

using namespace chess;

int main() {
    Board board = Board("3k4/8/5b1p/6p1/8/1P2B1P1/P1P2P1P/1BK5 w - - 12 62");
    std::cout << "Evaluate now" << std::endl;
    std::cout << bishopValue(board, 330, Color::WHITE) << std::endl;
    std::cout << bishopValue(board, 330, Color::BLACK) << std::endl;

    return 0;
}
