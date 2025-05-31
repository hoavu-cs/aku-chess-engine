#include "count_min.h"
#include "murmurmash1.h"
#include <limits>
#include <algorithm>

uint32_t MurmurPairHash::operator()(const std::pair<int, int>& p, uint32_t seed) const {
    int buf[2] = { p.first, p.second };
    return MurmurHash1(buf, sizeof(buf), seed);
}

CountMinSketchIntInt::CountMinSketchIntInt(size_t width, size_t depth, uint32_t base_seed)
    : width(static_cast<unsigned int>(width)),
      depth(static_cast<unsigned int>(depth)),
      table(depth, std::vector<unsigned int>(width, 0)),
      seeds(depth) {
    for (unsigned int i = 0; i < depth; ++i) {
        seeds[i] = base_seed + i * 0x5bd1e995;
    }
}

void CountMinSketchIntInt::insert(const std::pair<int, int>& item, unsigned int count) {
    for (unsigned int i = 0; i < depth; ++i) {
        uint32_t h = hasher(item, seeds[i]) % width;
        table[i][h] += count;
    }
}

unsigned int CountMinSketchIntInt::estimate(const std::pair<int, int>& item) const {
    unsigned int min_count = std::numeric_limits<unsigned int>::max();
    for (unsigned int i = 0; i < depth; ++i) {
        uint32_t h = hasher(item, seeds[i]) % width;
        min_count = std::min(min_count, table[i][h]);
    }
    return min_count;
}

void CountMinSketchIntInt::clear() {
    for (auto& row : table) {
        std::fill(row.begin(), row.end(), 0);
    }
}
