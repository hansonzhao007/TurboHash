#pragma once
#include <string>
#include <cstdint>
#include <cstring>
#include <stdlib.h>
#include <immintrin.h>
#include "bitset.h"
namespace lthash {


/**
 *  Description: Hash cell whose size is 256 byte. There are 28 slots in the cell.
 *  Format:
 *  | ----------------------- meta ------------------------| ----- slots ---- |
 *  | 4 byte bitmap | 28 byte: one byte hash for each slot | 8 byte * 28 slot |
 *  bitmap: 
 *      0  - 3  bit: not in use
 *      4  - 31 bit: indicate which slot is empty, 0: empty or deleted, 1: occupied
 *  one byte hash:
 *      8 bit hash for the slot
 *  slot:
 *      0  -  5 byte: the pointer used to point to DIMM where store the actual kv value
 *      6  -  7 byte: two hash byte for this slot (totally 3 byte is used as the hash)
*/ 
class CellMeta256 {
public:
    explicit CellMeta256(const char* rep) {
        meta_ = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rep));
        bitmap_ = *(const uint32_t*)(rep); // the lowest 32bit is used as bitmap
        bitmap_ &= 0x0FFFFFFF0;             // hidden the 0 - 3 bit in bitmap
    }

    // return a bitset, the slot that matches the hash is set to 1
    BitSet MatchBitSet(uint8_t hash) {
        auto bitset = _mm256_set1_epi8(hash);
        return BitSet(_mm256_movemask_epi8(_mm256_cmpeq_epi8(bitset, meta_)) &
                      (bitmap_) /* filter out empty slot*/);
    }

    // return a bitset, the slot that is ok for insertion
    BitSet EmptyBitSet() {
        return BitSet((~bitmap_) & 0x0FFFFFFF0);
    }

    BitSet OccupyBitSet() {
        return BitSet(bitmap_);
    }

    int OccupyCount() {
        return __builtin_popcount(bitmap_);
    }
    static uint32_t CellSize() {
        // cell size (include meta) in byte
        return 256;
    }

    static uint32_t SlotSize() {
        // slot count
        return 28;
    }

    static uint32_t BitMapType() {
        return 0;
    }

    static size_t size() {
        // the meta size in byte in current cell
        return 32;
    }

    std::string BitMapToString() {
        std::string res;
        char buffer[1024];
        uint64_t H2s[4];
        memcpy(H2s, &meta_, 32);
        sprintf(buffer, "bitmap: 0b%s - H2: 0x%016lx%016lx%016lx%016lx", print_binary(bitmap_).c_str(), H2s[3], H2s[2], H2s[1], H2s[0]);
        return buffer;
    }

    std::string print_binary(uint32_t bitmap)
    {
        char buffer[1024];
        const char *bit_rep[16] = {
            [ 0] = "0000", [ 1] = "0001", [ 2] = "0010", [ 3] = "0011",
            [ 4] = "0100", [ 5] = "0101", [ 6] = "0110", [ 7] = "0111",
            [ 8] = "1000", [ 9] = "1001", [10] = "1010", [11] = "1011",
            [12] = "1100", [13] = "1101", [14] = "1110", [15] = "1111",
        };
        sprintf(buffer, "%s%s%s%s%s%s%s%s", 
            bit_rep[(bitmap >> 28) & 0x0F],
            bit_rep[(bitmap >> 24) & 0x0F],
            bit_rep[(bitmap >> 20) & 0x0F],
            bit_rep[(bitmap >> 16) & 0x0F],
            bit_rep[(bitmap >> 12) & 0x0F],
            bit_rep[(bitmap >>  8) & 0x0F],
            bit_rep[(bitmap >>  4) & 0x0F],
            bit_rep[(bitmap >>  0) & 0x0F]
        );
        return buffer;
    }
    uint32_t bitmap() {
        return bitmap_;
    }
private:
    __m256i     meta_;      // 32 byte integer vector
    uint32_t    bitmap_;    // 1: occupied, 0: empty or deleted
};


/**
 *  Description: Hash cell whose size is 128 byte. There are 14 slots in the cell.
 *  Format:
 *  | ----------------------- meta ------------------------| ----- slots ---- |
 *  | 2 byte bitmap | 14 byte: one byte hash for each slot | 8 byte * 14 slot |
 *  bitmap: 
 *      0  -  1 bit: not in use
 *      2  - 15 bit: indicate which slot is empty, 0: empty or deleted, 1: occupied
 *  one byte hash:
 *      8 bit hash for the slot
 *  slot:
 *      0  -  5 byte: the pointer used to point to DIMM where store the actual kv value
 *      6  -  7 byte: two hash byte for this slot (totally 3 byte is used as the hash)
*/ 
class CellMeta128 {
public:
    explicit CellMeta128(const char* rep) {
        meta_ = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rep));
        bitmap_ = *(const uint32_t*)(rep); // the lowest 32bit is used as bitmap
        bitmap_ &= 0xFFFC;             // hide the 0, 1 bit in bitmap
    }

    // return a bitset, the position that matches the hash is set to 1
    BitSet MatchBitSet(uint8_t hash) {
        auto bitset = _mm_set1_epi8(hash);
        return BitSet(_mm_movemask_epi8(_mm_cmpeq_epi8(bitset, meta_)) &
                      (bitmap_) /* filter out empty slot*/);
    }

    // return a bitset, the position that is empty for insertion
    BitSet EmptyBitSet() {
        return BitSet((~bitmap_) & 0xFFFC);
    }

    BitSet OccupyBitSet() {
        return BitSet(bitmap_);
    }

    int OccupyCount() {
        return __builtin_popcount(bitmap_);
    }

    static uint32_t CellSize() {
        // cell size (include meta) in byte
        return 128;
    }

    static uint32_t SlotSize() {
        // slot count
        return 14;
    }

    static uint16_t BitMapType() {
        return 0;
    }

    static size_t size() {
        // the meta size in byte in current cell
        return 16;
    }

    std::string BitMapToString() {
        std::string res;
        char buffer[1024];
        uint64_t H2s[2];
        memcpy(H2s, &meta_, 16);
        sprintf(buffer, "bitmap: 0b%s - H2: 0x%016lx%016lx", print_binary(bitmap_).c_str(), H2s[1], H2s[0]);
        return buffer;
    }

    std::string print_binary(uint16_t bitmap)
    {
        char buffer[1024];
        const char *bit_rep[16] = {
            [ 0] = "0000", [ 1] = "0001", [ 2] = "0010", [ 3] = "0011",
            [ 4] = "0100", [ 5] = "0101", [ 6] = "0110", [ 7] = "0111",
            [ 8] = "1000", [ 9] = "1001", [10] = "1010", [11] = "1011",
            [12] = "1100", [13] = "1101", [14] = "1110", [15] = "1111",
        };
        sprintf(buffer, "%s%s%s%s", 
            bit_rep[(bitmap >> 12) & 0x0F],
            bit_rep[(bitmap >>  8) & 0x0F],
            bit_rep[(bitmap >>  4) & 0x0F],
            bit_rep[(bitmap >>  0) & 0x0F]
        );
        return buffer;
    }

    uint16_t bitmap() {
        return bitmap_;
    }

private:
    __m128i     meta_;      // 16 byte integer vector
    uint16_t    bitmap_;    // 1: occupied, 0: empty or deleted
};


}