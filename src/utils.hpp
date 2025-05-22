#include "chess.hpp"
#include <chrono>

using namespace chess; 

// Function declarations
inline std::string format_analysis(
    int depth,
    int bestEval,
    size_t totalNodeCount,
    size_t totalTableHit,
    const std::chrono::high_resolution_clock::time_point& startTime,
    const std::vector<Move>& PV,
    const Board& board
);
inline uint32_t fast_rand(uint32_t& seed);

// Function definitions
inline std::string format_analysis(
    int depth,
    int best_eval,
    size_t total_node_count,
    size_t total_table_hit,
    const std::chrono::high_resolution_clock::time_point& start_time,
    const std::vector<Move>& pv,
    const Board& board
) {
    std::string depth_str = "depth " + std::to_string(depth) + " seldepth " + std::to_string(std::max(size_t(depth), pv.size()));
    std::string score_str = "score cp " + std::to_string(best_eval);
    std::string node_str = "nodes " + std::to_string(total_node_count);
    std::string table_hit_str = "tableHit " + std::to_string(
        static_cast<double>(total_table_hit) / total_node_count
    );

    auto iteration_end_time = std::chrono::high_resolution_clock::now();
    std::string time_str = "time " + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(iteration_end_time - start_time).count()
    );

    std::string pv_str = "pv ";
    for (const auto& move : pv) {
        pv_str += uci::moveToUci(move, board.chess960()) + " ";
    }

    std::string analysis = "info " + depth_str + " " + score_str + " " + node_str + " " + time_str + " " + pv_str;
    return analysis;
}

inline uint32_t fast_rand(uint32_t& seed) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

