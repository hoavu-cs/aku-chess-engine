#pragma once
#include "chess.hpp"

using namespace chess;

enum NodeType {PV, CUT, ALL};

// Constants & global variables
constexpr int INF = 1000000;
constexpr int SZYZYGY_INF = 40000;
extern int table_size; // Maximum size of the transposition table

struct NodeData {
    int ply;
    bool nmp_ok; // flag to signal if nmp is allowed
    int root_depth; // maximum depth to search from the root
    NodeType node_type;
    Move excluded_move;
    int thread_id;
};

void initializeNNUE(std::string path);
void initializeTB(std::string path);
int negamax(Board& board, int depth, int alpha, int beta, std::vector<Move>& PV, NodeData& nodeInfo);
std::tuple<Move, int, int, std::vector<Move>> rootSearch(Board &board, int maxDepth, int timeLimit, int threadID);
Move lazysmpRootSearch(Board &board, int numThreads, int maxDepth, int timeLimit);
