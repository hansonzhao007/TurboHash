#pragma once
#include <inttypes.h>
#include <string>

namespace lthash {

#define LTHASH_H3_SIZE uint64_t
#define LTHASH_H2_SIZE uint8_t 
#define LTHASH_H1_SIZE uint16_t


// 64 bit hash value
// |  40 bit  |  8 bit  |  16 bit  |
// |    H3    |    H2   |    H1    |
// H3: is used to locate cell in each bucket, or it is index for associate
// H2: is the one byte hash in the meta
// H1: is the two byte hash stored in the non-used byte of the 8 byte-pointer
const int CONST_SHIFT = 1;
inline LTHASH_H3_SIZE H3(uint64_t hash) { return (hash  >> (24 + CONST_SHIFT));}
inline LTHASH_H2_SIZE H2(uint64_t hash) { return ((hash >> (16 + CONST_SHIFT)) & 0xFF);}
inline LTHASH_H1_SIZE H1(uint64_t hash) { return ((hash >> CONST_SHIFT) & 0xFFFF);}

/**
 *  AssociateMask is used to mod associate_count
*/
class ProbeWithinBucket {
public:
    ProbeWithinBucket(uint64_t initial_hash, uint32_t associate_mask, uint32_t bucket_i, uint32_t bucket_count) {
        h_               = initial_hash;
        associate_mask_  = associate_mask;
        associate_index_ = h_ & associate_mask_;
        bucket_i_        = bucket_i;
        probe_count_     = 0;
    }

    void reset() {
        associate_index_ = h_ & associate_mask_;
        probe_count_     = 0;
    }
    // indicate whether we have already probed all the assocaite cells
    operator bool() const {
        return probe_count_ <= associate_mask_;
    }

    void next() {
        associate_index_++;
        // assocaite_index mod AssociateCount, 
        // AssociateMask should be like 0b11
        associate_index_ &= associate_mask_;
        probe_count_++;
    }

    std::pair<uint32_t, uint32_t> offset() {
        return {bucket_i_, associate_index_};
    }

    static std::string name() {
        return "ProbeWithinBucket";
    }
private:
    uint64_t  h_;
    uint32_t  associate_mask_;
    uint32_t  associate_index_;
    uint32_t  bucket_i_;
    int       probe_count_;
};

/**
 *  Probe in one bucket and its next bucket
*/
class ProbeContinousTwoBucket{
public:
    ProbeContinousTwoBucket(uint64_t initial_hash, uint32_t associate_mask, uint32_t bucket_i, uint32_t bucket_count) {
        h_               = initial_hash;
        associate_mask_  = associate_mask;
        associate_index_ = h_ & associate_mask_;
        bucket_i_        = bucket_i;
        bucket_count_    = bucket_count;
        probe_count_     = 0;
    }

    void reset() {
        associate_index_ = h_ & associate_mask_;
        probe_count_     = 0;
    }
    // indicate whether we have already probed all the assocaite cells
    operator bool() const {
        return (probe_count_ >> 1) <= associate_mask_;
    }

    void next() {
        associate_index_++;
        // assocaite_index mod AssociateCount, 
        // AssociateMask should be like 0b11
        associate_index_ &= associate_mask_;
        if (probe_count_ == associate_mask_) {
            bucket_i_++;
            bucket_i_ %= bucket_count_;
        }
        probe_count_++;
    }

    static std::string name() {
        return "ProbeContinousTwoBucket";
    }

    std::pair<uint32_t, uint32_t> offset() {
        return {bucket_i_, associate_index_};
    }

private:
    uint64_t  h_;
    uint32_t  associate_mask_;
    uint32_t  associate_index_;
    uint32_t  bucket_i_;
    uint32_t  bucket_count_;
    int       probe_count_;
};


}