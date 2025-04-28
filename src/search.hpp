/*
* Author: Hoa T. Vu
* Created: December 1, 2024
* 
* Copyright (c) 2024 Hoa T. Vu
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to use,
* copy, modify, merge, publish, and distribute copies of the Software for 
* **non-commercial purposes only**, provided that the following conditions are met:
* 
* 1. The above copyright notice and this permission notice shall be included in
*    all copies or substantial portions of the Software.
* 2. Any use of this Software for commercial purposes **requires prior written
*    permission from the author, Hoa T. Vu**.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/


#pragma once

#include "chess.hpp"

using namespace chess;

enum NodeType {
    PV,
    CUT,
    ALL
};

// Constants & global variables
const int INF = 1000000;
const int SZYZYGY_INF = 40000;
extern int tableSize; // Maximum size of the transposition table

// Function Declarations
void initializeNNUE(std::string path);

void initializeTB(std::string path);

struct NodeInfo {
    int ply;
    bool leftMost;

    int checkExtensions;
    int singularExtensions;
    int oneMoveExtensions;

    bool doNMP;
    bool doSingularSearch;

    Move lastMove;
    NodeType nodeType;

    int threadID;
};

int negamax(Board& board, int depth, int alpha, int beta, std::vector<Move>& PV, NodeInfo& nodeInfo);

Move findBestMove(Board &board, int numThreads, int maxDepth, int timeLimit, bool quiet);

