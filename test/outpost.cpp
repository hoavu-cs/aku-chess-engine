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
    Board board = Board("1k6/8/p7/P1p5/2P1p1p1/6P1/P7/2K5 w - - 0 1");
    for (int i = 0; i < 64; i++) {
        if (isOutpost(board, i, Color::WHITE)) {
            char file = 'a' + i % 8;
            int rank = i / 8 + 1;
            std::cout << "Outpost for white at " <<  file << rank << std::endl;
        }
        
    }

    for (int i = 0; i < 64; i++) {
        if (isOutpost(board, i, Color::BLACK)) {
            char file = 'a' + i % 8;
            int rank = i / 8 + 1;
            std::cout << "Outpost for black at " << file << rank << std::endl;
        }
    }

    return 0;
}
