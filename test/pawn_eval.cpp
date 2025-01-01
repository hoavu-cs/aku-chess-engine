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
    Board board = Board("5k2/P1p5/7P/PP2P1p1/pPP2pP1/3P3p/8/5K2 w - - 0 1");
    std::vector<bool> openFiles(8, false);
    std::vector<bool> semiOpenFilesWhite(8, false);
    std::vector<bool> semiOpenFilesBlack(8, false);

    std::cout << "overall evaluation: " << evaluate(board) << std::endl;
    return 0;
}
