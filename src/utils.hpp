#include "chess.hpp"
#include <vector>
#include <chrono>
#include <atomic>

using namespace chess; 

extern std::atomic<uint64_t> benchmark_nodes;

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
inline void benchmark(int bench_depth, const std::vector<std::string>& benchmark_position, bool chess960); 

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
    std::string score_str = "score cp " + std::to_string(best_eval / 2);
    std::string node_str = "nodes " + std::to_string(total_node_count);
    std::string table_hit_str = "tableHit " + std::to_string(
        static_cast<double>(total_table_hit) / total_node_count
    );

    auto iteration_end_time = std::chrono::high_resolution_clock::now();
    auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(iteration_end_time - start_time).count();
    std::string time_str = "time " + std::to_string(time_ms);

    // Calculate nodes per second
    std::string nps_str = "nps ";
    if (time_ms > 0) {
        double time_seconds = static_cast<double>(time_ms) / 1000.0;
        size_t nps = static_cast<size_t>(total_node_count / time_seconds);
        nps_str += std::to_string(nps);
    } else {
        nps_str += "0";
    }

    std::string pv_str = "pv ";
    for (const auto& move : pv) {
        pv_str += uci::moveToUci(move, board.chess960()) + " ";
    }

    std::string analysis = "info " + depth_str + " " + score_str + " " + node_str + " " + nps_str + " " + time_str + " " + pv_str;
    return analysis;
}

inline uint32_t fast_rand(uint32_t& seed) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}
