#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <random>

#include "util/trace.h"

#define KEY_LEN ((15))

auto rng = std::default_random_engine {};

void GenerateRandomKeys(std::vector<size_t>& res, size_t min, size_t max, size_t count, int32_t seeds = 123) {
    res.resize(count);
    
    util::TraceUniform trace(seeds, min, max);
    
    for (size_t i = 0; i < count; ++i) {
        res[i] = trace.Next();
        if ((i & 0xFFFFF) == 0) {
            fprintf(stderr, "generate: %03d->seed: 0x%x\r", int(i >> 20), seeds);fflush(stderr);
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
    RandomKeyTrace(size_t count, int seed = random()) {
        count_ = count;
        keys_.resize(count);
        for (size_t i = 0; i < count; i++) {
            keys_[i] = i;
        }
        Randomize();
    }

    ~RandomKeyTrace() {
    }

    void Randomize(void) {
        printf("Randomize the trace...\r");
        fflush(nullptr);
        std::shuffle(std::begin(keys_), std::end(keys_), rng);
    }
    
    class RangeIterator {
    public:
        RangeIterator(std::vector<size_t>* pkey_vec, size_t start, size_t end):
            pkey_vec_(pkey_vec),                        
            end_index_(end), 
            cur_index_(start) { }

        inline bool Valid() {
            return (cur_index_ < end_index_);
        }

        inline size_t Next() {
            return (*pkey_vec_)[cur_index_++];
        }

        std::vector<size_t>* pkey_vec_;
        size_t end_index_;
        size_t cur_index_;
    };

    class Iterator {
    public:
        Iterator(std::vector<size_t>* pkey_vec, size_t start_index, size_t range):
            pkey_vec_(pkey_vec),
            range_(range),                        
            end_index_(start_index % range_), 
            cur_index_(start_index % range_),
            begin_(true)  
        {
            
        }

        Iterator(){}

        inline bool Valid() {
            return (begin_ || cur_index_ != end_index_);
        }

        inline size_t Next() {
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

        std::vector<size_t>* pkey_vec_;
        size_t range_;
        size_t end_index_;
        size_t cur_index_;
        bool   begin_;
    };

    Iterator trace_at(size_t start_index, size_t range) {
        return Iterator(&keys_, start_index, range);
    }

    RangeIterator Begin(void) {
        return RangeIterator(&keys_, 0, keys_.size());
    }

    RangeIterator iterate_between(size_t start, size_t end) {
        return RangeIterator(&keys_, start, end);
    }

    size_t count_;
    std::vector<size_t> keys_;
};


class RandomKeyTraceString {
public:
    RandomKeyTraceString(size_t count, int seed = random()) {
        count_ = count;
        keys_.resize(count);
        for (size_t i = 0; i < count; i++) {
            char buf[128];
            sprintf(buf, "%0.*lu", KEY_LEN, i);
            keys_[i] = std::string(buf, KEY_LEN);
        }
        Randomize();
    }

    ~RandomKeyTraceString() {
    }

    void Randomize(void) {
        printf("Randomize the trace...\r");
        fflush(nullptr);
        std::shuffle(std::begin(keys_), std::end(keys_), rng);
    }
    
    class RangeIterator {
    public:
        RangeIterator(std::vector<std::string>* pkey_vec, size_t start, size_t end):
            pkey_vec_(pkey_vec),                        
            end_index_(end), 
            cur_index_(start) { }

        inline bool Valid() {
            return (cur_index_ < end_index_);
        }

        inline std::string& Next() {
            return (*pkey_vec_)[cur_index_++];
        }

        std::vector<std::string>* pkey_vec_;
        size_t end_index_;
        size_t cur_index_;
    };

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

        Iterator(){}

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

    RangeIterator Begin(void) {
        return RangeIterator(&keys_, 0, keys_.size());
    }

    RangeIterator iterate_between(size_t start, size_t end) {
        return RangeIterator(&keys_, start, end);
    }

    size_t count_;
    std::vector<std::string> keys_;
};

