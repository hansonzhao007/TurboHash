#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <fstream>

#include "util/trace.h"

void GenerateRandomKeys(std::vector<std::string>& res, size_t min, size_t max, size_t count, int32_t seeds = 123) {
    res.resize(count);
    
    util::TraceUniform trace(seeds, min, max);
    
    for (size_t i = 0; i < count; ++i) {
        char buffer[128] = {0};
        sprintf(buffer, "key%017lu", trace.Next());
        res[i] = std::string(buffer);
        if ((i & 0xFFFFF) == 0) {
            fprintf(stderr, "generate%*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
        }
    }
    printf("\r");
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
        keys[i].append("key").append(std::to_string(i));
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
        count_ = count;
        GenerateRandomKeys(keys_, 0, 3000000000L, count_);
    }

    ~RandomKeyTrace() {
    }

    class Iterator {
    public:
        Iterator(std::vector<std::string>* pkey_vec, size_t start_index, size_t range):
            pkey_vec_(pkey_vec),
            range_(range),                        
            end_index_(start_index % range_), 
            cur_index_(start_index % range_),
            begin_(true)  
        {
            
        }

        inline bool Valid() {
            return (begin_ || cur_index_ != end_index_);
        }

        inline std::string& Next() {
            begin_ = false;
            size_t index = cur_index_;
            cur_index_++;
            if (cur_index_ >= range_) {
                cur_index_ = 0;
            }
            return (*pkey_vec_)[index];
        }

        std::string Info() {
            char buffer[128];
            sprintf(buffer, "valid: %s, cur i: %lu, end_i: %lu, range: %lu", Valid() ? "true" : "false", cur_index_, end_index_, range_);
            return buffer;
        }
        std::vector<std::string>* pkey_vec_;
        size_t range_;
        size_t end_index_;
        size_t cur_index_;
        bool   begin_;
    };

    Iterator trace_at(size_t start_index, size_t range) {
        return Iterator(&keys_, start_index, range);
    }
    
    size_t KeySize() {
        if (keys_.size() == 0) {
            return 0;
        }
        return keys_[0].size();
    }

    size_t count_;
    std::vector<std::string> keys_;
};

class RandomKeyTrace2 {
public:
    RandomKeyTrace2(size_t count) {
        seed_  = 31415926;
        count_ = count;
    }

    class Iterator {
    public:
        Iterator(size_t start_index, size_t range, int seed):
        trace_(seed, 0, 3000000000),
        range_(range),                        
        end_index_(start_index % range_), 
        cur_index_(start_index % range_),
        begin_(true)
        {
            trace_.Reset();
            // shift to start_index pos
            for (size_t i = 0; i < start_index; ++i)
                trace_.Next();
        }

        std::string Info() {
            char buffer[128];
            sprintf(buffer, "valid: %s, cur i: %lu, end_i: %lu, range: %lu", Valid() ? "true" : "false", cur_index_, end_index_, range_);
            return buffer;
        }
        
        inline bool Valid() {
            return begin_ || cur_index_ != end_index_;
        }

        inline std::string Next() {
            begin_ = false;
            std::string res = util::itostr(trace_.Next());
            cur_index_++;
            if (cur_index_ >= range_) {
                trace_.Reset();
                cur_index_ = 0;
            }
            return res;
        }

        inline int fast_mod(const int input, const int ceil) {
            // apply the modulo operator only when needed
            // (i.e. when the input is greater than the ceiling)
            return input >= ceil ? input % ceil : input;
            // NB: the assumption here is that the numbers are positive
        }
    private:
        util::TraceUniform trace_;
        size_t range_;
        size_t end_index_;
        size_t cur_index_;
        bool   begin_;
    };

    Iterator trace_at(size_t start_index, size_t range) {
        return Iterator(start_index, range, seed_);
    }
private:
    int seed_;
    size_t count_;
};