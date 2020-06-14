#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <fstream>

#include "util/trace.h"

std::string* GenerateRandomKeys(size_t min, size_t max, size_t count, int32_t seeds = 123) {
    std::string* res = new std::string[count];
    
    util::TraceUniform trace(seeds, min, max);
    
    for (size_t i = 0; i < count; ++i) {
        res[i] += "key" + std::to_string(trace.Next());
        if ((i & 0xFFFFF) == 0) {
            fprintf(stderr, "generate%*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
        }
    }
    printf("\r");
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
        if ((i & 0xFFFFF) == 0) {
            fprintf(stderr, "generate%*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
        }
    }
    printf("\r");
    return keys;
}

class RandomKeyTrace {
public:
    RandomKeyTrace(size_t count) {
        seed_  = 31415926;
        count_ = count;
    }

    class Iterator {
    public:
        Iterator(size_t start_index, size_t range, int seed):
        trace_(seed, 0, util::kRANDOM_RANGE),
        count_(range+1),                        // plus one, so Next() could generate rage # keys
        end_index_(start_index % range),        // make sure end_index < count
        cur_index_((start_index + 1) % range)   // make sure cur_index < count
        {
            // shift to start_index pos
            for (size_t i = 0; i < start_index; ++i)
                trace_.Next();
        }

        inline bool Valid() {
            return cur_index_ != end_index_;
        }

        inline util::Slice Next() {
            cur_index_++;
            cur_index_ = fast_mod(cur_index_, count_);
            return util::itostr(trace_.Next());
        }

        inline int fast_mod(const int input, const int ceil) {
            // apply the modulo operator only when needed
            // (i.e. when the input is greater than the ceiling)
            return input >= ceil ? input % ceil : input;
            // NB: the assumption here is that the numbers are positive
        }
    private:
        util::TraceUniform trace_;
        size_t count_;
        size_t end_index_;
        size_t cur_index_;
    };

    Iterator trace_at(size_t start_index, size_t range) {
        return Iterator(start_index, range, seed_);
    }
private:
    int seed_;
    size_t count_;
};