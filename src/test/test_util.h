#pragma once

#include <vector>
#include <string>
#include <algorithm>

#include "util/trace.h"

std::string* GenerateRandomKeys(size_t min, size_t max, size_t count, int32_t seeds = 123) {
    std::string* res = new std::string[count];
    
    util::TraceUniform trace(seeds, min, max);
    
    for (size_t i = 0; i < count; ++i) {
        res[i] += "key" + std::to_string(trace.Next());
    }
    return res;
}


int ShuffleFun(int i) {
  static util::Trace* trace = new util::TraceUniform(142857);
  return trace->Next() % i;
}

std::string* GenerateAllKeysInRange(size_t min, size_t max) {
    // generate keys in range [min, max]
    size_t count = max - min + 1;
    std::string* keys = new std::string[count];
    for (size_t i = min; i <= max; ++i) {
        keys[i] += "key" + std::to_string(i);
    }
    return keys;
}
