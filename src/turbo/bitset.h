#pragma once
#include <cstdint>

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
    BitSet& operator++() {
        // remove the lowest 1-bit
        bits_ &= (bits_ - 1);
        return *this;
    }
    explicit operator bool() const { return bits_ != 0; }
    int operator*() const { 
        // count the tailing zero bit
        return __builtin_ctz(bits_); 
    }
    BitSet begin() const { return *this; }
    BitSet end() const { return BitSet(0); }
    uint32_t bit() {
        return bits_;
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