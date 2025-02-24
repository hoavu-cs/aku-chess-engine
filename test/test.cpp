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
    Board board = Board("1r1q4/6pk/3P2pp/1p1Q4/p2P4/P6P/1P3P2/3R2K1 b - - 0 31");

    std::cout << "overall evaluation: " << evaluate(board) << std::endl;
    return 0;
}
