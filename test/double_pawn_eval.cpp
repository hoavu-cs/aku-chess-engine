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
    Board board = Board("7k/4pp2/8/8/2P5/P1P5/P1P2K2/8 w - - 0 1");
    std::cout << "Evaluate now" << std::endl;
    std::cout << pawnValue(board, 100, Color::WHITE) << std::endl;
    std::cout << pawnValue(board, 100, Color::BLACK) << std::endl;

    return 0;
}
