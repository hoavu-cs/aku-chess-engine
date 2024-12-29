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
    Board board = Board("5k2/3p4/6p1/P3Pp2/1PP5/3P3P/7P/5K2 w - - 0 1");
    std::vector<bool> openFiles(8, false);
    std::vector<bool> semiOpenFilesWhite(8, false);
    std::vector<bool> semiOpenFilesBlack(8, false);

    std::cout << "overall evaluation: " << evaluate(board) << std::endl;
    return 0;
}
