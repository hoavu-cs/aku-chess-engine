#pragma once
#include "chess.hpp"
#include <atomic>

using namespace chess;

enum NodeType {PV = 0, CUT = 1, ALL = 2};

// Constants & global variables
constexpr int INF = 1000000;
constexpr int SZYZYGY_INF = 40000;
extern int table_size; // Maximum size of the transposition table
extern bool stop_search; // To signal if the search should stop based on time control
extern std::atomic<bool> search_stopped; // Global stop flag for search based on UCI request


struct NodeData {
    int ply;
    bool nmp_ok; // flag to signal if nmp is allowed
    int root_depth; // maximum depth to search from the root
    NodeType node_type;
    Move excluded_move;
    int thread_id;
};

void reset_data();
bool initialize_nnue(std::string path);
int negamax(Board& board, int depth, int alpha, int beta, std::vector<Move>& PV, NodeData& node_data);
std::tuple<Move, int, int, std::vector<Move>> root_search(Board &board, int max_depth, int time_limit, int thread_id);
Move lazysmp_root_search(Board &board, int num_threads, int max_depth, int time_limit);


