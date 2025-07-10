#pragma once

#include <unordered_map>
#include <utility>
#include <cstdint>

// Hash function for pairs of int
struct PairHash {
    std::size_t operator()(const std::pair<int, int>& p) const {
        return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
    }
};

// Misra-Gries data structure for pair<int, int> items
class MisraGriesIntInt {
public:
    MisraGriesIntInt(int k) : k(k) {}

    void insert(const std::pair<int, int>& item) {
        if (counter.count(item)) {
            counter[item]++;
        } else if (counter.size() < k - 1) {
            counter[item] = 1;
        } else {
            for (auto it = counter.begin(); it != counter.end(); ) {
                if (--(it->second) <= 0)
                    it = counter.erase(it);
                else
                    ++it;
            }
        }
    }

    void clear() {
        // Clear the counter map
        counter = {};
    }

    int get_count(const std::pair<int, int>& item) const {
        auto it = counter.find(item);
        return (it != counter.end()) ? it->second : 0;
    }

    const std::unordered_map<std::pair<int, int>, int, PairHash>& get_counts() const {
        return counter;
    }

    void for_each_item(const std::function<void(std::pair<int, int>&, int&)>& func) {
        for (auto& entry : counter) {
            func(const_cast<std::pair<int, int>&>(entry.first), entry.second);
        }
    }

private:
    int k;
    std::unordered_map<std::pair<int, int>, int, PairHash> counter;
};

// Misra-Gries for 64-bit integers items
class MisraGriesU64 {
public:
    MisraGriesU64(int k) : k(k) {}

    void insert(uint64_t item) {
        if (counter.count(item)) {
            counter[item]++;
        } else if (counter.size() < k - 1) {
            counter[item] = 1;
        } else {
            for (auto it = counter.begin(); it != counter.end(); ) {
                if (--(it->second) == 0)
                    it = counter.erase(it);
                else
                    ++it;
            }
        }
    }

    void clear() {
        counter = {};
    }

    int get_count(uint64_t item) const {
        auto it = counter.find(item);
        return (it != counter.end()) ? it->second : 0;
    }

    const std::unordered_map<uint64_t, int>& get_counts() const {
        return counter;
    }

private:
    int k;
    std::unordered_map<uint64_t, int> counter;
};
