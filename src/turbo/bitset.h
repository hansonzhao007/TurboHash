#pragma once
#include <cstdint>
#include "hash_function.h"
namespace turbo {

// Usage example:
// BitSet bitset(0x05); 
// for (int i : bitset) {
//     printf("i: %d\n", i);
// }
// this will print out 0, 2
class BitSet {
public:
    BitSet():
        bits_(0) {}

    explicit BitSet(uint32_t bits): bits_(bits) {}

    BitSet(const BitSet& b) {
        bits_ = b.bits_;
    }
    
    inline BitSet& operator++() {
        // remove the lowest 1-bit
        bits_ &= (bits_ - 1);
        return *this;
    }

    inline explicit operator bool() const { return bits_ != 0; }

    inline int operator*() const { 
        // count the tailing zero bit
        return __builtin_ctz(bits_); 
    }

    inline BitSet begin() const { return *this; }

    inline BitSet end() const { return BitSet(0); }

    inline uint32_t bit() {
        return bits_;
    }

    inline int pickOne() const {
        // method 1: random pick any one
        // return SelectRandomSetBit(bits_);

        // method 2: randomly pick first one or next one(if possible)
        bool jump = wyhash64() & 0x1;
        if (!jump ||
            __builtin_popcount(bits_) == 1) {
            return __builtin_ctz(bits_);
        }

        return __builtin_ctz(bits_ & (bits_ - 1));
    }

private:
    friend bool operator==(const BitSet& a, const BitSet& b) {
        return a.bits_ == b.bits_;
    }
    friend bool operator!=(const BitSet& a, const BitSet& b) {
        return a.bits_ != b.bits_;
    }
    uint32_t bits_;
};

}