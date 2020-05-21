#pragma once
#include <string>
#include <cstdint>
#include <stdlib.h>

#include "bitset.h"
#include "hash_function.h"
#include "cell_meta.h"
#include "probe_strategy.h"
#include "util/slice.h"
#include "util/env.h"

#define LTHASH_DEBUG_OUT

namespace lthash {

using Slice = util::Slice; 

union HashSlot
{
    /* data */
    const void* entry;       // 8B
    struct {
        uint16_t none[3];
        uint16_t H1;
    } meta;
};

inline uint64_t H3(uint64_t hash) { return (hash >> 25);}
inline uint8_t  H2(uint64_t hash) { return ((hash >> 17) & 0xFF);}
inline uint16_t H1(uint64_t hash) { return ((hash >> 1 ) & 0xFFFF);}

// the inline hash function
// 64 bit hash value
// |  40 bit  |  8 bit  |  16 bit  |
// |    H3    |    H2   |    H1    |
// H3: is used to locate cell in each bucket, or it is index for associate
// H2: is the one byte hash in the meta
// H1: is the two byte hash stored in the non-used byte of the 8 byte-pointer
struct PartialHash {
    PartialHash(uint64_t hash, const Slice& key) {
        H3_ = H3(hash);
        H2_ = H2(hash);
        H1_ = H1(hash);
    };

    uint64_t H3_;
    uint8_t  H2_;
    uint16_t H1_;
    uint16_t key_fp_;
};

class SlotInfo {
public:
    uint32_t bucket;
    uint32_t associate;
    int slot;
    uint16_t H1;
    uint8_t  H2;
    SlotInfo(uint32_t b, uint32_t a, int s, uint16_t h1, uint8_t h2):
        bucket(b),
        associate(a),
        slot(s),
        H1(h1),
        H2(h2) {}
    SlotInfo():
        bucket(0),
        associate(0),
        slot(0),
        H1(0),
        H2(0) {}
};

class HashTable {
public:
    virtual bool Put(const Slice& key, const Slice& value) = 0;
    virtual bool Get(const Slice& key, std::string* value) = 0;
    virtual void Delete(const Slice& key) = 0;
    virtual double LoadFactor() = 0;
    virtual size_t Size() = 0;
    virtual std::string ProbeStrategyName() = 0;
};

/**
 *  Format:
 *              | bucket 0 | bucket 1 | ... | bucket C |
 *  associate 0 |  cell    |          |     |          |
 *  associate 1 |          |          |     |          |
 *  ....        |          |          |     |          |
 *  associate R |          |          |     |          |
 * 
 *  Cell:
 *  |    meta   |       slots        |
 *  meta: store the meta info, such as which slot is empty
 *  slots: each slot is a 8-byte value, that stores the pointer to kv location.
 *         the higher 2-byte is used as partial hash
 *  Description:
 *  DramHashTable has bucket_count * associate_count cells
*/
template <class CellMeta = CellMeta128, class ProbeStrategy = ProbeWithinBucket>
class DramHashTable: public HashTable {
public:
    // Change 'Hasher' to another hash implementation to change hash function
    // The 'Hasher' implementation has to implement the following function: 
    // ---- uint64_t hash ( const void * key, int len, unsigned int seed) ----
    using Hasher = MurMurHash3;
    const int kCellSize = CellMeta::CellSize();
    class iterator {
        friend class DramHashTable;
    };

    explicit DramHashTable(uint32_t bucket_count, uint32_t associate_count):
        bucket_count_(bucket_count),
        bucket_mask_(bucket_count - 1),
        associate_count_(associate_count),
        associate_mask_(associate_count - 1),
        capacity_(bucket_count * associate_count * CellMeta::SlotSize()) {
        if (!isPowerOfTwo(bucket_count) ||
            !isPowerOfTwo(associate_count)) {
            printf("the hash table size setting is wrong. bucket: %u, associate: %u\n", bucket_count, associate_count);
            exit(1);
        }
        // calloc will initialize the memory space to zero
        // cells_  = (char*)calloc(kCellSize, bucket_count * associate_count);

        cells_  = (char*)aligned_alloc(kCellSize, bucket_count * associate_count * kCellSize);
        memset(cells_, 0, bucket_count * associate_count * kCellSize);
        size_     = 0;
    }

    explicit DramHashTable(void* addr, uint32_t bucket_count, uint32_t associate_count, size_t size):
        cells_(addr),
        bucket_count_(bucket_count),
        bucket_mask_(bucket_count - 1),
        associate_count_(associate_count),
        associate_mask_(associate_mask_ - 1),
        capacity_(bucket_count * associate_count * CellMeta::SlotSize()),
        size_(size) {
        if (!isPowerOfTwo(bucket_count) ||
            !isPowerOfTwo(associate_count)) {
            printf("the hash table size setting is wrong. bucket: %u, associate: %u\n", bucket_count, associate_count);
            exit(1);
        }
    }

    ~DramHashTable() {
        free(cells_);
    }

    // value format:
    // |        2B      |       6B          |
    // | partial hash   |  pointer to DIMM  |
    // Here the value should be the pointer to DIMM
    bool Put(const Slice& key, const Slice& value) {
        // step 1: find a conflict slot in cell or empty slot
        // step 2: store the value to media, update the slot
        // step 3: highly unlikely. There is no valid slot. (need expand the hash table)

        // step 1:
        auto res = findSlotForInsert(key);
        if (res.second) {
            // step 2:
            void* media_pos = storeValueToMedia(key, value);

            // insert H1 to higher 2 byte of the slot
            HashSlot slot;
            slot.entry = media_pos;
            slot.meta.H1 = res.first.H1;
            updateSlot(res.first, slot);

            // update meta one byte hash(H2), and set bitmap
            updateMeta(res.first);
            ++size_;
            return true;
        }

        // step 3:
        return false;
    }

    // Return the value of the key if key exists
    bool Get(const Slice& key, std::string* value) {
        auto res = findSlot(key);
        if (res.second) {
            // find a key in hash table

            return true;
        }
        return false;
    }

    void Delete(const Slice& key) {

    }

    double LoadFactor() {
        return (double) size_ / capacity_;
    }

    size_t Size() { return size_;}

    // Persist entire dram hash to persistent memory
    void Persist(void* pmemaddr) {

    }

    std::string ProbeStrategyName() {
        return ProbeStrategy::name();
    }

    std::string PrintBucketMeta(uint32_t bucket_i) {
        std::string res;
        char buffer[1024];
        sprintf(buffer, "----- bucket %10u -----\n", bucket_i);
        res += buffer;
        ProbeStrategy probe(0, associate_mask_, bucket_i, bucket_count_);
        uint32_t i = 0;
        int count_sum = 0;
        while (probe) {
            char* cell_addr = locateCell(probe.offset());
            CellMeta meta(cell_addr);
            int count = meta.OccupyCount(); 
            sprintf(buffer, "\t%4u - 0x%12lx: %s. Cell valid slot count: %d\n", i++, (uint64_t)cell_addr, meta.BitMapToString().c_str(), count);
            res += buffer;
            probe.next();
            count_sum += count;
        }
        sprintf(buffer, "\tBucket valid slot count: %d\n", count_sum);
        res += buffer;
        return res;
    }
    
    void PrintAllMeta() {
        for (size_t b = 0; b < bucket_count_; ++b) {
            printf("%s\n", PrintBucketMeta(b).c_str());
        }
    }
private:
    // set meta after inserting value to slot
    void setCellMeta(size_t i, uint32_t hash) {

    }

    inline std::pair<uint64_t, uint64_t> twoHashValue(const Slice& key) {
        auto res = Hasher::hash(key.data(), key.size(), key.size());
        return res;
    }

    inline size_t locateBucket(const uint64_t& hash) {
        return hash & bucket_mask_;
    }

    // offset.first: bucket index
    // offset.second: associate index
    inline char* locateCell(const std::pair<size_t, size_t> offset) {
        return cells_ + (associate_count_ * kCellSize * offset.first +  // locate the bucket
                         offset.second * kCellSize);                    // locate the associate cell
    }
    // inline char* locateCell(size_t bucket_i, size_t associate_i) {
    //     // cells in the same bucket are allocate in adjecent memory address
    //     return cells_ + (associate_count_ * kCellSize * bucket_i +  // locate the bucket
    //                      associate_i * kCellSize);                  // locate the associate cell
    // }
  
    inline HashSlot* locateSlot(const char* cell_addr, int slot_i) {
        return (HashSlot*)(cell_addr + slot_i * 8);
    }

    inline void updateSlot(const SlotInfo& info, const HashSlot& slot) {
        // update slot content
        char* cell_addr = locateCell({info.bucket, info.associate});
        HashSlot* slot_pos = locateSlot(cell_addr, info.slot);
        *slot_pos = slot;
    }

    inline void updateMeta(const SlotInfo& info) {
        // update cell meda, set bitmap and one byte hash(H2)
        char* cell_addr = locateCell({info.bucket, info.associate});
        decltype(CellMeta::BitMapType())* bitmap = (decltype(CellMeta::BitMapType())*)cell_addr;
        // set bitmap
        *bitmap = (*bitmap) | (1 << info.slot);
        // set H2
        cell_addr += info.slot;
        *cell_addr = info.H2;
    }

    bool slotKeyEqual(const HashSlot& slot, const Slice& key) {
        // TODO (xingsheng): passing a functor or something
        auto tmp = slot;
        tmp.meta.H1 = 0;
        return Slice((const char*)tmp.entry, key.size()) == key;
    }

    const Slice extraceSlice(const HashSlot& slot, size_t len) {
        auto tmp = slot;
        tmp.meta.H1 = 0;
        return Slice((const char*)tmp.entry, len);
    }

    // Store the value to media and return the pointer to the media position
    // where the value stores
    inline void* storeValueToMedia(const Slice& key, const Slice& value) {
        // TODO (xingsheng): implement store functor
        void* buffer = malloc(key.size());
        memcpy(buffer, key.data(), key.size());
        return buffer;
    }

    // Find a valid slot for insertion
    // Return: std::pair
    //      first: the slot info that should insert the key
    //      second: whether we can find a empty slot to insert
    // Node: Only when the second value is true, can we insert this key
    std::pair<SlotInfo, bool> findSlotForInsert(const Slice& key) {
        auto hash_two = twoHashValue(key);
        uint32_t bucket_i = locateBucket(hash_two.first);
        auto associate_hash = hash_two.second;
        PartialHash partial_hash(associate_hash, key);
        ProbeStrategy probe(partial_hash.H3_, associate_mask_, bucket_i, bucket_count_);
        
        while (probe) {
            char* cell_addr = locateCell(probe.offset());
            CellMeta meta(cell_addr);
            // locate if there is any H2 match in this cell
            for (int i : meta.MatchBitSet(partial_hash.H2_)) {
                // i is the slot index in current cell, each slot
                // occupies 8-byte

                // locate the slot reference
                const HashSlot& slot = *locateSlot(cell_addr, i);

                // compare if the H1 partial hash is equal
                if (likely(slot.meta.H1 == partial_hash.H1_)) {
                    // compare the actual key pointed by the pointer 
                    // with the parameter key
                    if (likely(slotKeyEqual(slot, key))) {
                        return {{probe.offset().first, probe.offset().second, i, partial_hash.H1_, partial_hash.H2_}, true};
                    }
                }
            }
            auto empty_bitset = meta.EmptyBitSet(); 
            if (likely(empty_bitset)) {
                // there is empty slot for insertion, so we return a slot
                for (int i : empty_bitset) {
                    return {{probe.offset().first, probe.offset().second, i, partial_hash.H1_, partial_hash.H2_}, true};
                }
            }
            // probe the next cell in the same bucket
            probe.next(); 
        }

        #if 0
        #ifdef LTHASH_DEBUG_OUT
        printf("Fail to find one empty slot\n");
        printf("%s\n", PrintBucketMeta(bucket_i).c_str());
        #endif
        #endif
        // only when all the probes fail and there is no empty slot
        // exists in this bucket. 
        return {{}, false};
    }

    std::pair<SlotInfo, bool> findSlot(const Slice& key) {
        auto hash_two = twoHashValue(key);
        uint32_t bucket_i = locateBucket(hash_two.first);
        auto associate_hash = hash_two.second;
        PartialHash partial_hash(associate_hash, key);
        ProbeStrategy probe(partial_hash.H3_, associate_mask_, bucket_i, bucket_count_);

        while (probe) {
            char* cell_addr = locateCell(probe.offset());
            CellMeta meta(cell_addr);
            // locate if there is any H2 match in this cell
            for (int i : meta.MatchBitSet(partial_hash.H2_)) {
                // i is the slot index in current cell, each slot
                // occupies 8-byte

                // locate the slot reference
                const HashSlot& slot = *locateSlot(cell_addr, i);

                // compare if the H1 partial hash is equal
                if (likely(slot.meta.H1 == partial_hash.H1_)) {
                    // compare the actual key pointed by the pointer 
                    // with the parameter key
                    if (likely(slotKeyEqual(slot, key))) {
                        return {{probe.offset().first, probe.offset().second, i, partial_hash.H1_, partial_hash.H2_} , true};
                    }
                    #ifdef LTHASH_DEBUG_OUT
                    else {
                        printf("H1 conflict. Slot (%8u, %3u, %2d) bitmap: %s. Search key: %15s, H2: 0x%2x, H1: 0x%4x, Slot key: %15s, H1: 0x%4x\n", 
                            probe.offset().first, 
                            probe.offset().second, 
                            i, 
                            meta.BitMapToString().c_str(),
                            key.ToString().c_str(),
                            partial_hash.H2_,
                            partial_hash.H1_,
                            extraceSlice(slot, key.size()).ToString().c_str(),
                            slot.meta.H1
                            );
                    }
                    #endif
                }
            }
            // if this cell still has empty slot, then it means the key
            // does't exist.
            if (likely(meta.EmptyBitSet())) {
                return {{}, false};
            }
            probe.next();
        }
        // after all the probe, no key exist
        return {{}, false};
    }

    inline bool isPowerOfTwo(uint32_t n) {
        return (n != 0 && __builtin_popcount(n) == 1);
    }

private:
    char*         cells_ = nullptr;
    const size_t  bucket_count_ = 0;
    const size_t  bucket_mask_  = 0;
    const size_t  associate_count_ = 0;
    const size_t  associate_mask_  = 0;
    const size_t  capacity_ = 0;
    size_t        size_ = 0;
};

}