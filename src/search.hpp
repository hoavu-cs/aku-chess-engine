#pragma once
#include "chess.hpp"

using namespace chess;

enum NodeType {PV, CUT, ALL};

// Constants & global variables
constexpr int INF = 1000000;
constexpr int SZYZYGY_INF = 40000;
extern int tableSize; // Maximum size of the transposition table

struct NodeInfo {
    int ply;
    bool nmpOk; // flag to signal if nmp is allowed
    int rootDepth; // maximum depth to search from the root
    NodeType nodeType;
    int threadID;
};

void initializeNNUE(std::string path);
void initializeTB(std::string path);
int negamax(Board& board, int depth, int alpha, int beta, std::vector<Move>& PV, NodeInfo& nodeInfo);
Move rootSearch(Board &board, int maxDepth, int timeLimit, int threadID);

