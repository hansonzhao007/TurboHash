#pragma once

#include <vector>
#include <string>
#include <algorithm>

#include "util/trace.h"

std::vector<std::string> GenerateRandomKeys(size_t min, size_t max, size_t count, int32_t seeds = 123) {
    std::vector<std::string> res(count, "key");
    
    util::TraceUniform trace(seeds, min, max);
    
    for (size_t i = 0; i < count; ++i) {
        res[i] += std::to_string(trace.Next());
    }
    return res;
}


int ShuffleFun(int i) {
  static util::Trace* trace = new util::TraceUniform(142857);
  return trace->Next() % i;
}

std::vector<std::string> GenerateAllKeysInRange(size_t min, size_t max) {
    // generate keys in range [min, max]
    size_t count = max - min + 1;
    std::vector<std::string> res(count, "key");
    for (size_t i = min; i <= max; ++i) {
        res[i] += std::to_string(i);
    }
    return res;
}
