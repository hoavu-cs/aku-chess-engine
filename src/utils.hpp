#include "chess.hpp"
#include <chrono>

using namespace chess; 

//--------------------------------------------------------------------------------------------
// Function declarations
//--------------------------------------------------------------------------------------------

//  UCI analysis string formatting
inline std::string formatAnalysis(
    int depth,
    int bestEval,
    size_t totalNodeCount,
    size_t totalTableHit,
    const std::chrono::high_resolution_clock::time_point& startTime,
    const std::vector<Move>& PV,
    const Board& board
);

// fast random number generator
inline uint32_t fastRand(uint32_t& seed);

//--------------------------------------------------------------------------------------------
// Function definitions
//--------------------------------------------------------------------------------------------

inline std::string formatAnalysis(
    int depth,
    int bestEval,
    size_t totalNodeCount,
    size_t totalTableHit,
    const std::chrono::high_resolution_clock::time_point& startTime,
    const std::vector<Move>& PV,
    const Board& board
) {
    std::string depthStr = "depth " + std::to_string(std::max(size_t(depth), PV.size()));
    std::string scoreStr = "score cp " + std::to_string(bestEval);
    std::string nodeStr = "nodes " + std::to_string(totalNodeCount);
    std::string tableHitStr = "tableHit " + std::to_string(
        static_cast<double>(totalTableHit) / totalNodeCount
    );

    auto iterationEndTime = std::chrono::high_resolution_clock::now();
    std::string timeStr = "time " + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(iterationEndTime - startTime).count()
    );

    std::string pvStr = "pv ";
    for (const auto& move : PV) {
        pvStr += uci::moveToUci(move, board.chess960()) + " ";
    }

    std::string analysis = "info " + depthStr + " " + scoreStr + " " + nodeStr + " " + timeStr + " " + pvStr;
    return analysis;
}

inline uint32_t fastRand(uint32_t& seed) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

