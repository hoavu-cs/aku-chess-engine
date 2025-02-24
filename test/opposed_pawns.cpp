#include "chess.hpp"
#include "evaluation.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <tuple> 
#include <string>
#include <vector>
#include <algorithm>

using namespace chess;

int main() {
    Board board = Board("2k5/8/3p1p2/3P1P2/1pP5/1P5p/7P/2K5 w - - 0 1");

    evaluate(board);
    

    return 0;
}
