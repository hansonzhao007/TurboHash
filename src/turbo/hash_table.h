#pragma once
#include <string>
#include <cstdint>
#include <stdlib.h>
#include <atomic>
#include <vector>
#include <libpmem.h>
#include <algorithm>
#include <numeric>

#include "allocator.h"
#include "bucket_iterator.h"
#include "format.h"
#include "bitset.h"
#include "media.h"
#include "hash_function.h"
#include "cell_meta.h"
#include "probe_strategy.h"
#include "util/slice.h"
#include "util/coding.h"
#include "util/status.h"
#include "util/prefetcher.h"
#include "util/env.h"

// #define LTHASH_DEBUG_OUT

namespace turbo {

class HashTable {
public:
    virtual ~HashTable() {}
    virtual bool Put(const util::Slice& key, const util::Slice& value) = 0;
    virtual bool Get(const std::string& key, std::string* value) = 0;
    virtual bool Find(const std::string& key, uint64_t& data_offset) = 0;
    virtual void Delete(const Slice& key) = 0;
    virtual double LoadFactor() = 0;
    virtual size_t Size() = 0;
    virtual void MinorReHashAll() = 0;
    virtual void DebugInfo() = 0;
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
template <class CellMeta = CellMeta128, class ProbeStrategy = ProbeWithinBucket, class Media = DramMedia, int kAssociateSizeLimit = 32768>
class DramHashTable: public HashTable {
public:
    using Slice = util::Slice; 
    using Status = util::Status;
    using Hasher = MurMurHash;
    using value_type = std::pair<util::Slice, util::Slice>;
    explicit DramHashTable(uint32_t bucket_count, uint32_t associate_count):
        bucket_count_(bucket_count),
        bucket_mask_(bucket_count - 1),
        capacity_(bucket_count * associate_count * CellMeta::SlotSize()),
        size_(0) {
        if (!isPowerOfTwo(bucket_count) ||
            !isPowerOfTwo(associate_count)) {
            printf("the hash table size setting is wrong. bucket: %u, associate: %u\n", bucket_count, associate_count);
            exit(1);
        }

        buckets_ = new BucketMeta[bucket_count];
        buckets_mem_block_ids_ = new int[bucket_count];
        for (size_t i = 0; i < bucket_count; ++i) {
            auto res = mem_allocator_.AllocateNoSafe(associate_count);
            memset(res.second, 0, associate_count * kCellSize);
            buckets_[i].Reset(res.second, associate_count);
            buckets_mem_block_ids_[i] = res.first;
        }
        
    }

    void DebugInfo() {
        printf("%s\n", PrintMemAllocator().c_str());
    }
    std::string PrintMemAllocator() {
        return mem_allocator_.ToString();
    }

    void MinorReHashAll() override {
        // allocate new mem space together
        int* old_mem_block_ids = (int*)malloc(bucket_count_ * sizeof(int));
        char** new_mem_block_addr = (char**)malloc(bucket_count_ * sizeof(char*));
        for (size_t i = 0; i < bucket_count_; ++i) {
            auto bucket_meta = locateBucket(i);
            uint32_t new_associate_size = bucket_meta.AssociateSize() << 1;
            // if current bucket cannot enlarge any more, continue
            if (unlikely(new_associate_size > kAssociateSizeLimit)) {
                printf("Cannot rehash bucket %lu\n", i);
                continue;
            }
            auto res = mem_allocator_.AllocateNoSafe(new_associate_size);
            if (res.second == nullptr) {
                printf("Error\n");
                exit(1);
            }
            old_mem_block_ids[i] = buckets_mem_block_ids_[i];
            buckets_mem_block_ids_[i] = res.first;
            new_mem_block_addr[i] = res.second;
            capacity_.fetch_add(bucket_meta.AssociateSize() * CellMeta::SlotSize());
            // printf("bucket %lu. Allocate address: 0x%lx. size: %lu\n", i, res.second, new_associate_size);
        }

        // auto rehash_start = util::Env::Default()->NowMicros();
        // rehash for all the buckets
        int rehash_thread = 4;
        // printf("Rehash threads: %d\n", rehash_thread);
        
        std::vector<std::thread> workers(rehash_thread);
        std::vector<size_t> add_capacity(rehash_thread, 0);
        std::atomic<size_t> rehash_count(0);
        for (int t = 0; t < rehash_thread; t++) {
            workers[t] = std::thread([&, t]
            {
                size_t start_b = bucket_count_ / rehash_thread * t;
                size_t end_b   = start_b + bucket_count_ / rehash_thread;
                size_t counts  = 0;
                // printf("Rehash bucket [%lu, %lu)\n", start_b, end_b);
                for (size_t i = start_b; i < end_b; ++i) {
                    counts += MinorRehash(i, new_mem_block_addr[i]);
                }
                rehash_count.fetch_add(counts, std::memory_order_relaxed);
            });
        }
        std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
        {
            t.join();
        });
        // auto rehash_end = util::Env::Default()->NowMicros();
    
        // size_t rehash_count = std::accumulate(counts.begin(), counts.end(), 0);
        // printf("Real rehash speed: %f Mops/s\n", (double)rehash_count / (rehash_end - rehash_start));
        // release the old mem block space
        for (size_t i = 0; i < bucket_count_; ++i) {
            mem_allocator_.ReleaseNoSafe(old_mem_block_ids[i]);
        }

        free(old_mem_block_ids);
        free(new_mem_block_addr);
        printf("Rehash %lu entries\n", rehash_count.load());
    }

    // return the associate index and slot index
    inline std::pair<uint16_t, uint8_t> findNextSlotInRehash(uint8_t* slot_vec, uint16_t h1, uint16_t associate_mask) {
        uint16_t ai = h1 & associate_mask;
        int loop_count = 0;

        // find next cell that is not full yet
        uint32_t SLOT_MAX_RANGE = CellMeta::SlotMaxRange(); 
        while (slot_vec[ai] >= SLOT_MAX_RANGE) {
            // because we use linear probe, if this cell is full, we go to next cell
            ai += ProbeStrategy::PROBE_STEP; 
            loop_count++;
            if (unlikely(loop_count > ProbeStrategy::MAX_PROBE_LEN)) {
                printf("ERROR!!! Even we rehash this bucket, we cannot find a valid slot within %d probe\n", ProbeStrategy::MAX_PROBE_LEN);
                exit(1);
            }
            if (ai > associate_mask) {
                ai &= associate_mask;
            }
        }
        return {ai, slot_vec[ai]++};
    }

    size_t MinorRehash(int bi, char* new_bucket_addr) {
        size_t count = 0;
        auto bucket_meta = locateBucket(bi);
        uint32_t old_associate_size = bucket_meta.AssociateSize();

        // create new bucket and initialize its meta
        uint32_t new_associate_size      = old_associate_size << 1;
        uint32_t new_associate_size_mask = new_associate_size - 1;
        if (!isPowerOfTwo(new_associate_size)) {
            printf("rehash bucket is not power of 2. %u\n", new_associate_size);
            exit(1);
        }
        if (new_bucket_addr == nullptr) {
            perror("rehash alloc memory fail\n");
            exit(1);
        }
        BucketMeta new_bucket_meta(new_bucket_addr, new_associate_size);
     
        // reset all cell's meta data
        for (size_t i = 0; i < new_associate_size; ++i) {
            char* des_cell_addr = new_bucket_addr + (i << kCellSizeLeftShift);
            memset(des_cell_addr, 0, CellMeta::size());
        }

        // iterator old bucket and insert slots info to new bucket
        // old: |11111111|22222222|33333333|44444444|   
        //       ========>
        // new: |1111    |22222   |333     |4444    |1111    |222     |33333   |4444    |
        
        // record next avaliable slot position of each cell within new bucket for rehash
        uint8_t* slot_vec = (uint8_t*)malloc(new_associate_size);
        memset(slot_vec, CellMeta::StartSlotPos(), new_associate_size);
        // std::vector<uint8_t> slot_vec(new_associate_size, CellMeta::StartSlotPos());   
        BucketIterator<CellMeta> iter(bi, bucket_meta.Address(), bucket_meta.AssociateSize()); 

        while (iter.valid()) { // iterator every slot in this bucket
            count++;
            // obtain old slot info and slot content
            auto res = *iter;

            // update bitmap, H2, H1 and slot pointer in new bucket
            // 1. find valid slot in new bucket
            auto valid_slot = findNextSlotInRehash(slot_vec, res.first.H1, new_associate_size_mask);
            
            // 2. move the slot from old bucket to new bucket
            char* des_cell_addr = new_bucket_addr + (valid_slot.first << kCellSizeLeftShift);
            res.first.slot      = valid_slot.second;
            res.first.associate = valid_slot.first;
            if (res.first.slot >= CellMeta::SlotMaxRange() || res.first.associate >= new_associate_size) {
                printf("rehash fail: %s\n", res.first.ToString().c_str());
                exit(1);
            }
            moveSlot(des_cell_addr, res.first, res.second);            
            ++iter;
        }
        // replace old bucket meta in buckets_
        buckets_[bi] = new_bucket_meta;
        free(slot_vec);
        return count;
    }

    ~DramHashTable() {

    }

    bool Put(const util::Slice& key, const util::Slice& value) {
        // calculate hash value of the key
        size_t hash_value = Hasher::hash(key.data(), key.size());

        // store the kv pair to media
        void* media_offset = Media::Store(key, value);

        // update DRAM index, thread safe
        return insertSlot(key, hash_value, media_offset);
    }

    // Return the entry if key exists
    bool Get(const std::string& key, std::string* value) {
        size_t hash_value = Hasher::hash(key.data(), key.size());
        auto res = findSlot(key, hash_value);
        if (res.second) {
            // find a key in hash table
            auto datanode = Media::ParseData(res.first.entry);
            if (datanode.first  == kTypeValue) {
                value->assign(datanode.second.second.data(), datanode.second.second.size());
                return true;
            }
            else {
                // this key has been deleted
                return false;
            }
        }
        return false;
    }

    bool Find(const std::string& key, uint64_t& data_offset) {
        size_t hash_value = Hasher::hash(key.data(), key.size());
        auto res = findSlot(key, hash_value);
        data_offset = res.first.entry;
        return res.second;
    }

    void Delete(const Slice& key) {
        assert(key.size() != 0);
        printf("%s\n", key.ToString().c_str());
    }

    double LoadFactor() {
        return (double) size_.load(std::memory_order_relaxed) / capacity_.load(std::memory_order_relaxed);
    }

    size_t Size() { return size_.load(std::memory_order_relaxed);}

    // Persist entire dram hash to persistent memory
    void Persist(void* pmemaddr) {

    }

    void IterateValidBucket() {
        printf("Iterate Valid Bucket\n");
        for (size_t i = 0; i < bucket_count_; ++i) {
            auto bucket_meta = locateBucket(i);
            BucketIterator<CellMeta> iter(i, bucket_meta.Address(), bucket_meta.info.associate_size);
            if (iter.valid()) {
                printf("%s\n", PrintBucketMeta(i).c_str());
            }
        }
    }

    void IterateBucket(uint32_t i) {
        auto bucket_meta = locateBucket(i);
        BucketIterator<CellMeta> iter(i, bucket_meta.Address(), bucket_meta.AssociateSize());
        while (iter.valid()) {
            auto res = (*iter);
            SlotInfo& info = res.first;
            info.bucket = i;
            HashSlot& slot = res.second;
            auto datanode = Media::ParseData(slot.entry);
            printf("%s, addr: %16lx. key: %.8s, value: %s\n", 
                info.ToString().c_str(),
                slot.entry, 
                datanode.second.first.ToString().c_str(),
                datanode.second.second.ToString().c_str());
            ++iter;
        }
    }

    void IterateAll() {
        size_t count = 0;
        for (size_t i = 0; i < bucket_count_; ++i) {
            auto bucket_meta = locateBucket(i);
            BucketIterator<CellMeta> iter(i, bucket_meta.Address(), bucket_meta.AssociateSize());
            while (iter.valid()) {
                auto res = (*iter);
                SlotInfo& info = res.first;
                HashSlot& slot = res.second;
                auto datanode = Media::ParseData(slot.entry);
                printf("%s, addr: %16lx. key: %.8s, value: %s\n", 
                    info.ToString().c_str(),
                    slot.entry, 
                    datanode.second.first.ToString().c_str(),
                    datanode.second.second.ToString().c_str());
                ++iter;
                count++;
            }
        }
        printf("iterato %lu entries. total size: %lu\n", count, size_.load(std::memory_order_relaxed));
    }

    std::string ProbeStrategyName() {
        return ProbeStrategy::name();
    }

    std::string PrintBucketMeta(uint32_t bucket_i) {
        std::string res;
        char buffer[1024];
        BucketMeta meta = locateBucket(bucket_i);
        sprintf(buffer, "----- bucket %10u -----\n", bucket_i);
        res += buffer;
        ProbeStrategy probe(0, meta.AssociateMask(), bucket_i);
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
        sprintf(buffer, "\tBucket %u: valid slot count: %d. Usage Ratio: %f\n", bucket_i, count_sum, (double)count_sum / (CellMeta::SlotSize() * meta.AssociateSize()));
        res += buffer;
        return res;
    }
    
    void PrintAllMeta() {
        printf("Print All Bucket Meta\n");
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
    inline uint32_t bucketIndex(uint64_t hash) {
        return hash & bucket_mask_;
    }

    inline BucketMeta locateBucket(uint32_t bi) {
        return buckets_[bi];
    }

    // offset.first: bucket index
    // offset.second: associate index
    inline char* locateCell(const std::pair<size_t, size_t>& offset) {
        return  buckets_[offset.first].Address() +      // locate the bucket
                (offset.second << kCellSizeLeftShift);  // locate the associate cell
    }
  
    inline HashSlot* locateSlot(const char* cell_addr, int slot_i) {
        return (HashSlot*)(cell_addr + (slot_i << 3));
    }

    // used in rehash function, move slot to new cell_addr
    inline void moveSlot(char* des_cell_addr, const SlotInfo& des_info, const HashSlot& src_slot) {
        // move slot content, including H1 and pointer
        HashSlot* des_slot = locateSlot(des_cell_addr, des_info.slot);
        *des_slot = src_slot;
        // obtain bitmap
        decltype(CellMeta::BitMapType())* bitmap = (decltype(CellMeta::BitMapType())*)des_cell_addr;
        // set H2
        des_cell_addr += des_info.slot; // move to H2 address
        *des_cell_addr = des_info.H2;   // update  H2
        // set bitmap
        *bitmap = (*bitmap) | (1 << des_info.slot);
    }
    
    inline void updateSlotAndMeta(char* cell_addr, const SlotInfo& info, void* media_offset) {
        // set bitmap, 1 byte H2, 2 byte H1, 6 byte pointer

        // set 2 byte H1 and 6 byte pointer
        HashSlot* slot_pos = locateSlot(cell_addr, info.slot);
        slot_pos->entry = (uint64_t)media_offset;
        slot_pos->H1 = info.H1;
       
        // obtain bitmap
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
        bool retry_find = false;
        do { // concurrent insertion may find same position for insertion, retry insertion if neccessary
            auto res = findSlotForInsert(key, hash_value);

            if (res.second) { // find a valid slot
                char* cell_addr = locateCell({res.first.bucket, res.first.associate});
                
                SpinLockScope<0> lock_scope((turbo_bitspinlock*)cell_addr); // Lock current cell
                
                CellMeta meta(cell_addr);   // obtain the meta part
                
                if (likely(!meta.Occupy(res.first.slot) ||  // if the slot is not occupied or
                    res.first.equal_key)) {                 // the slot has same key, update the slot

                    // update slot content (including pointer and H1), H2 and bitmap
                    updateSlotAndMeta(cell_addr, res.first, media_offset);
                    if (!res.first.equal_key) size_.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
                else { // current slot has been occupied by another concurrent thread, retry.
                    // #ifdef LTHASH_DEBUG_OUT
                    printf("retry find slot. %s\n", key.ToString().c_str());
                    // #endif
                    retry_find = true;
                }
            }
            else { // cannot find a valid slot for insertion, rehash current bucket then retry
                // printf("=========== Need Rehash ==========\n");
                printf("%s\n", PrintBucketMeta(res.first.bucket).c_str());
                break;
            }
        } while (retry_find);
        
        return false;
    }

    inline bool slotKeyEqual(const HashSlot& slot, const Slice& key) {
        Slice res = Media::ParseKey(reinterpret_cast<void*>(slot.entry));
        return res == key;
    }

    inline const Slice extractSlice(const HashSlot& slot, size_t len) {
        return  Media::ParseKey(reinterpret_cast<void*>(slot.entry));
    }

    
    // Find a valid slot for insertion
    // Return: std::pair
    //      first: the slot info that should insert the key
    //      second: whether we can find a valid(empty or belong to the same key) slot to insert
    // Node: Only when the second value is true, can we insert this key
    inline std::pair<SlotInfo, bool> findSlotForInsert(const Slice& key, size_t hash_value) {
        PartialHash partial_hash(hash_value);
        uint32_t bucket_i = bucketIndex(partial_hash.bucket_hash_);
        auto bucket_meta = locateBucket(bucket_i);
        ProbeStrategy probe(partial_hash.H1_, bucket_meta.AssociateMask(), bucket_i);

        int probe_count = 0; // limit probe times
        while (probe && (probe_count++ < ProbeStrategy::MAX_PROBE_LEN)) {
            auto offset = probe.offset();
            char* cell_addr = locateCell(offset);
            CellMeta meta(cell_addr);

            // if this is a update request, we overwrite existing slot
            for (int i : meta.MatchBitSet(partial_hash.H2_)) {  // if there is any H2 match in this cell (H2 is 8-byte)
                                                                // i is the slot index in current cell, each slot occupies 8-byte
                // locate the slot reference
                const HashSlot& slot = *locateSlot(cell_addr, i);

                if (likely(slot.H1 == partial_hash.H1_)) {  // compare if the H1 partial hash is equal (H1 is 16-byte)
                    if (likely(slotKeyEqual(slot, key))) {  // compare if the slot key is equal
                        return {{   offset.first,           /* bucket */
                                    offset.second,          /* associate */
                                    i,                      /* slot */
                                    partial_hash.H1_,       /* H1 */ 
                                    partial_hash.H2_,       /* H2 */
                                    true},                  /* equal_key */
                                true};
                    }
                    else {                                  // two key is not equal, go to next slot
                        #ifdef LTHASH_DEBUG_OUT
                        Slice slot_key =  extractSlice(slot, key.size());
                        printf("H1 conflict. Slot (%8u, %3u, %2d) bitmap: %s. Insert key: %15s, 0x%016lx, Slot key: %15s, 0x%016lx\n", 
                            offset.first, 
                            offset.second, 
                            i, 
                            meta.BitMapToString().c_str(),
                            key.ToString().c_str(),
                            hash_value,
                            slot_key.ToString().c_str(),
                            Hasher::hash(slot_key.data(), slot_key.size())
                            );
                        #endif
                    }
                }
            }

            // return an empty slot for new insertion
            auto empty_bitset = meta.EmptyBitSet(); 
            if (likely(empty_bitset)) {
                // for (int i : empty_bitset) { // there is empty slot, return its meta

                    return {{   offset.first,           /* bucket */
                                offset.second,          /* associate */
                                empty_bitset.pickOne(), /* slot */
                                partial_hash.H1_,       /* H1 */
                                partial_hash.H2_,       /* H2 */
                                false /* a new slot */}, 
                            true};
                // }
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
        return {{
                    bucket_i,
                    0,
                    0, 
                    partial_hash.H1_, 
                    partial_hash.H2_, 
                    false /* a new slot */},
                false};
    }
    
    inline std::pair<HashSlot, bool> findSlot(const Slice& key, size_t hash_value) {
        PartialHash partial_hash(hash_value);
        uint32_t bucket_i = bucketIndex(partial_hash.bucket_hash_);
        auto bucket_meta = locateBucket(bucket_i);
        ProbeStrategy probe(partial_hash.H1_,  bucket_meta.AssociateMask(), bucket_i);

        int probe_count = 0; // limit probe times
        while (probe && (probe_count++ < ProbeStrategy::MAX_PROBE_LEN)) {
            auto offset = probe.offset();
            char* cell_addr = locateCell(offset);
            CellMeta meta(cell_addr);
            
            for (int i : meta.MatchBitSet(partial_hash.H2_)) {  // Locate if there is any H2 match in this cell
                                                                // i is the slot index in current cell, each slot occupies 8-byte
                // locate the slot reference
                const HashSlot& slot = *locateSlot(cell_addr, i);

                if (likely(slot.H1 == partial_hash.H1_)) { // Compare if the H1 partial hash is equal.
                    if (likely(slotKeyEqual(slot, key))) {      // If the slot key is equal to search key. SlotKeyEqual is very expensive 
                        return {slot, true};
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
                        //     slot.H1
                        //     );
                        
                        Slice slot_key =  extractSlice(slot, key.size());
                        printf("H1 conflict. Slot (%8u, %3u, %2d) bitmap: %s. Search key: %15s, 0x%016lx, Slot key: %15s, 0x%016lx\n", 
                            offset.first, 
                            offset.second, 
                            i, 
                            meta.BitMapToString().c_str(),
                            key.ToString().c_str(),
                            hash_value,
                            slot_key.ToString().c_str(),
                            Hasher::hash(slot_key.data(), slot_key.size())
                            );
                        #endif
                    }
                    
                }
            }

            // if this cell still has empty slot, then it means the key does't exist.
            if (likely(meta.EmptyBitSet())) {
                return {{0, 0}, false};
            }
            
            probe.next();
        }
        // after all the probe, no key exist
        return {{0, 0}, false};
    }

    inline bool isPowerOfTwo(uint32_t n) {
        return (n != 0 && __builtin_popcount(n) == 1);
    }

private:
    MemAllocator<CellMeta, kAssociateSizeLimit> mem_allocator_;
    BucketMeta*   buckets_;
    int*          buckets_mem_block_ids_;
    const size_t  bucket_count_ = 0;
    const size_t  bucket_mask_  = 0;
    std::atomic<size_t> capacity_;
    std::atomic<size_t> size_;

    const int       kCellSize = CellMeta::CellSize();
    const int       kCellSizeLeftShift = CellMeta::CellSizeLeftShift;
    const size_t    kMaxLogFileSize = 4LU << 30;        // 4 GB
};

}