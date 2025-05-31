#pragma once

#include <vector>
#include <utility>
#include <cstdint>

class CountMinSketchIntInt;

struct MurmurPairHash {
    uint32_t operator()(const std::pair<int, int>& p, uint32_t seed) const;
};

class CountMinSketchIntInt {
public:
    CountMinSketchIntInt(size_t width, size_t depth, uint32_t base_seed = 42);

    void insert(const std::pair<int, int>& item, unsigned int count = 1);
    unsigned int estimate(const std::pair<int, int>& item) const;
    void clear();

private:
    unsigned int width;
    unsigned int depth;
    std::vector<std::vector<unsigned int>> table;
    std::vector<uint32_t> seeds;
    MurmurPairHash hasher;
};

