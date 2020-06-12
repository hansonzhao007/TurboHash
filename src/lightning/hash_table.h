#pragma once
#include <string>
#include <cstdint>
#include <stdlib.h>
#include <atomic>
#include <vector>

#include "format.h"
#include "bitset.h"
#include "hash_function.h"
#include "cell_meta.h"
#include "probe_strategy.h"
#include "util/slice.h"
#include "util/coding.h"
#include "util/status.h"
#include "util/prefetcher.h"

// #define LTHASH_DEBUG_OUT

namespace lthash {
const size_t kMaxLogFileSize = 512 << 20;   // 2 GB
using Slice = util::Slice; 
using Status = util::Status;

union HashSlot
{
    /* data */
    const void* entry;       // 8B
    struct {
        uint16_t none[3];
        uint16_t H1;
    } meta;
};

// 64 bit hash function is used to locate initial position of keys
// |         4 B       |         4 B       |
// |   bucket hash     |    associate hash |
//                     | 1B | 1B |    2B   |
//                     | H3 | H2 |    H1   |
// H3: index within each bucket
// H2: 1 byte hash for parallel comparison
// H1: 2 byte partial key

#define BUCKET_H_SIZE uint32_t
#define LTHASH_H3_SIZE uint8_t
#define LTHASH_H2_SIZE uint8_t 
#define LTHASH_H1_SIZE uint16_t

struct PartialHash {
    PartialHash(uint64_t hash):
        bucket_hash_(hash >> 32),
        H1_(         hash & 0xFFFF),
        H2_( (hash >> 16) & 0xFF),
        H3_( (hash >> 24) & 0xFF) {
    };
    BUCKET_H_SIZE  bucket_hash_;
    // H1: 2 byte partial key
    LTHASH_H1_SIZE H1_;
    // H2: 1 byte hash for parallel comparison
    LTHASH_H2_SIZE H2_;
    // H3: index within each bucket
    LTHASH_H3_SIZE H3_;
};

class SlotInfo {
public:
    uint32_t bucket;
    uint32_t associate;
    int slot;
    LTHASH_H1_SIZE H1;
    LTHASH_H2_SIZE H2;
    bool equal_key;
    SlotInfo(uint32_t b, uint32_t a, int s, LTHASH_H1_SIZE h1, LTHASH_H2_SIZE h2, bool euqal):
        bucket(b),
        associate(a),
        slot(s),
        H1(h1),
        H2(h2),
        equal_key(euqal) {}
    SlotInfo():
        bucket(0),
        associate(0),
        slot(0),
        H1(0),
        H2(0),
        equal_key(false) {}
};


class HashTable {
public:
    virtual bool Put(const std::string& key, const std::string& value) = 0;
    virtual bool Get(const std::string& key, std::string* value) = 0;
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
    using Hasher = MurMurHash;
    const int kCellSize = CellMeta::CellSize();
    class iterator {
        friend class DramHashTable;
    };

    explicit DramHashTable(uint32_t bucket_count, uint32_t associate_count, bool use_h1_locate_cell):
        bucket_count_(bucket_count),
        bucket_mask_(bucket_count - 1),
        associate_count_(associate_count),
        associate_mask_(associate_count - 1),
        capacity_(bucket_count * associate_count * CellMeta::SlotSize()),
        locate_cell_with_h1_(use_h1_locate_cell),
        cur_log_offset_(0) {
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

        // queue init
        size_t QUEUE_SIZE = 1 << 10;
        queue_.resize(QUEUE_SIZE);
        queue_size_mask_ = QUEUE_SIZE - 1;
        queue_head_for_log_ = 0;
        queue_head_ = 0;
        queue_tail_ = 0;
    }

    explicit DramHashTable(void* addr, uint32_t bucket_count, uint32_t associate_count, size_t size):
        cells_(addr),
        bucket_count_(bucket_count),
        bucket_mask_(bucket_count - 1),
        associate_count_(associate_count),
        associate_mask_(associate_mask_ - 1),
        capacity_(bucket_count * associate_count * CellMeta::SlotSize()),
        size_(size)
         {
        if (!isPowerOfTwo(bucket_count) ||
            !isPowerOfTwo(associate_count)) {
            printf("the hash table size setting is wrong. bucket: %u, associate: %u\n", bucket_count, associate_count);
            exit(1);
        }
    }

    ~DramHashTable() {
        free(cells_);
    }
    

    bool Put(const std::string& key, const std::string& value) {
        // // step 1: put the entry to queue_, thread safe
        // // obtain current queue tail and advance queue tail atomically
        // uint32_t queue_tail = queue_tail_.fetch_add(1, std::memory_order_relaxed);
        // // add kv pair to queue
        // uint32_t queue_index = queue_tail & queue_size_mask_;
        // queue_[queue_index] = {key, value};

        // // step 2: wait until we obtain the ownership. spinlock
        // // will wait until queue_index is equal to queue_head_.
        // // means it is my turn
        // while (queue_tail != queue_head_.load(std::memory_order_relaxed));
        // // {{{ ======= atomic process start
        // // step 3: calculate the offset in the log and release the onwership to next request. (atomic)
        // // record :=
        // // 1B: value type
        // // 4B: key len
        // //   : key
        // // 4B: value len
        // //   : value
        // // 1B: checksum
        // size_t record_size = key.size() + value.size() + 10;
        // // obtain the log_offset for current put request
        // char* log_offset = cur_log_offset_ + cur_pmem_initial_addr_;
        // // if log offset is larger than file size, we create a new log file
        // // and reset log_offset
        // if (cur_log_offset_ + record_size > kMaxLogFileSize) {
        //     cur_log_offset_ = 0;
        //     log_offset = 0;
        //     printf("Create new log\n");
        // }
        // cur_log_offset_ += record_size;
        // // ======= atomic process end }}}
        // // release spinlock by adding queue_head
        // queue_head_.fetch_add(1, std::memory_order_relaxed);

        // step 4: write record to log_offset
        size_t hash_value = HashValue(key);
        void* media_offset = storeValueToMedia(key, value, 0);

        // step 5: update DRAM index
        return insertSlot(key, hash_value, media_offset);
    }

    // Return the entry if key exists
    bool Get(const std::string& key, std::string* value) {
        size_t hash_value = HashValue(key);
        auto res = findSlot(key, hash_value);
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

    std::string PrintBucket(uint32_t bucket_i) {
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

    void PrintHashTable() {
        for (int b = 0; b < bucket_count_; ++b) {
            printf("%s\n", PrintBucketMeta(b).c_str());

        }
    }

private:
    char* createNewLogFile() {

    }
    inline size_t HashValue(const Slice& key) {
        return Hasher::hash(key.data(), key.size());
    }

    inline uint32_t locateBucket(const uint64_t& hash) {
        return hash & bucket_mask_;
    }

    // offset.first: bucket index
    // offset.second: associate index
    inline char* locateCell(const std::pair<size_t, size_t> offset) {
        return cells_ + (associate_count_ * kCellSize * offset.first +  // locate the bucket
                         offset.second * kCellSize);                    // locate the associate cell
    }
  
    inline HashSlot* locateSlot(const char* cell_addr, int slot_i) {
        return (HashSlot*)(cell_addr + slot_i * 8);
    }

    inline void updateSlotAndMeta(const SlotInfo& info, void* media_offset) {
        // update slot content, H1 and pointer
        char* cell_addr = locateCell({info.bucket, info.associate});
        HashSlot* slot_pos = locateSlot(cell_addr, info.slot);
        slot_pos->entry = media_offset;
        slot_pos->meta.H1 = info.H1;
       
        // update cell bitmap and one byte hash(H2)
        decltype(CellMeta::BitMapType())* bitmap = (decltype(CellMeta::BitMapType())*)cell_addr;
        // set H2
        cell_addr += info.slot; // move cell_addr one byte hash position
        *cell_addr = info.H2;   // update the one byte hash

        // add a fence here. 
        // Make sure the bitmap is updated after H2
        // Prevent StoreStore reorder
        // https://www.modernescpp.com/index.php/fences-as-memory-barriers
        // https://preshing.com/20130922/acquire-and-release-fences/
        std::atomic_thread_fence(std::memory_order_release);

        // set bitmap
        *bitmap = (*bitmap) | (1 << info.slot);
    }


    inline bool insertSlot(const Slice& key, size_t hash_value, void* media_offset) {
        // atomically update the media offset to relative slot
        bool retry_find = false;
        do {
            // concurrent insertion may find the same position
            auto res = findSlotForInsert(key, hash_value);

            if (res.second) {
                // find a valid slot
                
                // obtain the cell lock
                char* cell_addr = locateCell({res.first.bucket, res.first.associate});
                SpinLockScope((lthash_bitspinlock*)cell_addr);
                
                CellMeta meta(cell_addr);
                if (likely(!meta.Occupy(res.first.slot) || res.first.equal_key)) {
                    // if the slot is not occupied or the slot has same key with request
                    // we update the slot 

                    // update slot content (including pointer and H1), H2 and bitmap
                    updateSlotAndMeta(res.first, media_offset);
                    if (!res.first.equal_key) ++size_;
                    return true;
                }
                else {
                    // if current slot already occupies by another 
                    // concurrent thread, we retry find slot.
                    printf("retry find slot. %s\n", key.ToString().c_str());
                    // printf("%s\n", PrintBucketMeta(res.first.bucket).c_str());
                    retry_find = true;
                }
            }
        } while (retry_find);
        
        return false;
    }

    inline bool slotKeyEqual(const HashSlot& slot, const Slice& key) {
        // TODO (xingsheng): passing a functor or something
        auto tmp = slot;
        tmp.meta.H1 = 0;
        return memcmp(tmp.entry, key.data(), key.size()) == 0;
    }

    const Slice extractSlice(const HashSlot& slot, size_t len) {
        auto tmp = slot;
        tmp.meta.H1 = 0;
        return Slice((const char*)tmp.entry, strlen((const char*)tmp.entry));
    }

    // Store the value to media and return the pointer to the media position
    // where the value stores
    inline void* storeValueToMedia(const Slice& key, const Slice& value, char* log_offset) {
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
    std::pair<SlotInfo, bool> findSlotForInsert(const Slice& key, size_t hash_value) {
        PartialHash partial_hash(hash_value);
        uint32_t bucket_i = locateBucket(partial_hash.bucket_hash_);
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
                        return {{probe.offset().first, probe.offset().second, i, partial_hash.H1_, partial_hash.H2_, true}, true};
                    }
                    else {
                        #ifdef LTHASH_DEBUG_OUT
                        Slice slot_key =  extractSlice(slot, key.size());
                        printf("H1 conflict. Slot (%8u, %3u, %2d) bitmap: %s. Insert key: %15s, 0x%016lx, Slot key: %15s, 0x%016lx\n", 
                            probe.offset().first, 
                            probe.offset().second, 
                            i, 
                            meta.BitMapToString().c_str(),
                            key.ToString().c_str(),
                            hash_value,
                            slot_key.ToString().c_str(),
                            HashValue(slot_key)
                            );
                        #endif
                    }
                }
            }
            auto empty_bitset = meta.EmptyBitSet(); 
            if (likely(empty_bitset)) {
                // there is empty slot for insertion, so we return a slot
                for (int i : empty_bitset) {
                    return {{probe.offset().first, probe.offset().second, i, partial_hash.H1_, partial_hash.H2_, false}, true};
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

    inline void PrefetchSlotKey(const char* cell_addr, int slot_i) {
        if (slot_i < CellMeta::SlotSize()) {
            // only when the slot index is smaller than size limit, we do prefetch
            HashSlot slot = *locateSlot(cell_addr, slot_i);
            slot.meta.H1 = 0;
            util::PrefetchForRead(slot.entry, util::Locality_No_Temporal);
        }
    }

    
    std::pair<SlotInfo, bool> findSlot(const Slice& key, size_t hash_value) {
        PartialHash partial_hash(hash_value);
        uint32_t bucket_i = locateBucket(partial_hash.bucket_hash_);
        ProbeStrategy probe(partial_hash.H3_, associate_mask_, bucket_i, bucket_count_);

        while (probe) {
            char* cell_addr = locateCell(probe.offset());
            CellMeta meta(cell_addr);
            // locate if there is any H2 match in this cell
            for (int i : meta.MatchBitSet(partial_hash.H2_)) {
                // i is the slot index in current cell, each slot
                // occupies 8-byte

                // PrefetchSlotKey(cell_addr, i);
                // locate the slot reference
                const HashSlot& slot = *locateSlot(cell_addr, i);
                util::PrefetchForRead((void*)((uint64_t)slot.entry & 0x0000FFFFFFFFFFFF), util::Locality_1);

                // compare if the H1 partial hash is equal
                if (likely(slot.meta.H1 == partial_hash.H1_)) {
                    // compare the actual key pointed by the pointer 
                    // with the parameter key
                    if (likely(slotKeyEqual(slot, key))) {
                        // slotKeyEqual is very expensive 
                        return {{probe.offset().first, probe.offset().second, i, partial_hash.H1_, partial_hash.H2_, true} , true};
                    }
                    else {
                        #ifdef LTHASH_DEBUG_OUT
                        // printf("H1 conflict. Slot (%8u, %3u, %2d) bitmap: %s. Search key: %15s, H2: 0x%2x, H1: 0x%4x, Slot key: %15s, H1: 0x%4x\n", 
                        //     probe.offset().first, 
                        //     probe.offset().second, 
                        //     i, 
                        //     meta.BitMapToString().c_str(),
                        //     key.ToString().c_str(),
                        //     partial_hash.H2_,
                        //     partial_hash.H1_,
                        //     extraceSlice(slot, key.size()).ToString().c_str(),
                        //     slot.meta.H1
                        //     );
                        
                        Slice slot_key =  extractSlice(slot, key.size());
                        printf("H1 conflict. Slot (%8u, %3u, %2d) bitmap: %s. Search key: %15s, 0x%016lx, Slot key: %15s, 0x%016lx\n", 
                            probe.offset().first, 
                            probe.offset().second, 
                            i, 
                            meta.BitMapToString().c_str(),
                            key.ToString().c_str(),
                            hash_value,
                            slot_key.ToString().c_str(),
                            HashValue(slot_key)
                            );
                        #endif
                    }
                    
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
    bool          locate_cell_with_h1_ = false;

    // ----- circular queue for synchronizing put requests -----
    std::vector<std::pair<Slice, Slice>> queue_;
    uint32_t              queue_size_mask_;
    std::atomic<uint32_t> queue_tail_;
    std::atomic<uint32_t> queue_head_;
    uint32_t              queue_head_for_log_;       // used for back ground logging thread

    // ----- for pmem append log -----
    // guarded by spinlock (while loop)
    uint64_t      log_seq_ = 0;
    size_t        cur_log_offset_ = 0;
    char*         cur_pmem_initial_addr_ = nullptr;
};

}