#pragma once
#include <string>
#include <cstdint>
#include <stdlib.h>
#include <atomic>
#include <vector>
#include <libpmem.h>

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
    virtual void Delete(const Slice& key) = 0;
    virtual double LoadFactor() = 0;
    virtual size_t Size() = 0;
    virtual void WarmUp() = 0;
    virtual void ReHashAll() = 0;
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
template <class CellMeta = CellMeta128, class ProbeStrategy = ProbeWithinBucket, class Media = DramMedia>
class DramHashTable: public HashTable {
public:
    using Slice = util::Slice; 
    using Status = util::Status;
    using Hasher = MurMurHash;
    using value_type = std::pair<util::Slice, util::Slice>;

    explicit DramHashTable(uint32_t bucket_count, uint32_t associate_count):
        bucket_count_(bucket_count),
        bucket_mask_(bucket_count - 1),
        associate_size_(associate_count),
        associate_mask_(associate_count - 1),
        capacity_(bucket_count * associate_count * CellMeta::SlotSize()),
        cur_log_offset_(0),
        size_(0) {
        if (!isPowerOfTwo(bucket_count) ||
            !isPowerOfTwo(associate_count)) {
            printf("the hash table size setting is wrong. bucket: %u, associate: %u\n", bucket_count, associate_count);
            exit(1);
        }

        buckets_ = new BucketMeta[bucket_count];
        cells_   = (char*) aligned_alloc(kCellSize, bucket_count_ * associate_size_ * kCellSize);
        memset(cells_, 0, bucket_count_ * associate_size_ * kCellSize);
        for (size_t i = 0; i < bucket_count; ++i) {
            buckets_[i].__addr = cells_ + i * associate_size_ * kCellSize;
            buckets_[i].info.associate_size = associate_size_;
        }
        size_     = 0;

        // queue init
        size_t QUEUE_SIZE = 1 << 10;
        queue_.resize(QUEUE_SIZE);
        queue_size_mask_ = QUEUE_SIZE - 1;
        queue_head_for_log_ = 0;
        queue_head_ = 0;
        queue_tail_ = 0;
    }

    void WarmUp() override {
        char * addr = nullptr;
        for (int i = 0; i < 1000; i++) {
            addr = buckets_[i].__addr;
        }
        // printf("finish warm up: %lx\n", (uint64_t)addr);
    }
    void ReHashAll() override {
        // enlarge space 
        size_t count = 0;
        size_t new_associate_size = associate_size_ * 2;
        if (new_associate_size > 65536) {
            printf("cannot rehash. current associate size reach maximum size 65536: %lu\n", associate_size_);
            return;
        }
        char* new_cells = (char*) aligned_alloc(kCellSize, bucket_count_ * new_associate_size * kCellSize);
        for (size_t i = 0; i < bucket_count_; ++i) {
            count += Rehash(i, new_cells + i * new_associate_size * kCellSize);
        }
        free(cells_);
        cells_ = new_cells;
        associate_size_ = new_associate_size;
        capacity_ *= 2;
        printf("Rehash %lu entries\n", count);
    }

    // return the associate index and slot index
    inline std::pair<uint16_t, uint8_t> findNextSlotInRehash(std::vector<uint8_t>& slot_vec, uint16_t h1, uint16_t associate_mask) {
        uint16_t ai = h1 & associate_mask;
        int loop_count = 0;

        // find next cell that is not full yet
        uint32_t SLOT_MAX_RANGE = CellMeta::SlotMaxRange(); 
        while (slot_vec[ai] >= SLOT_MAX_RANGE) {
            ai++;
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

    size_t Rehash(int bi, char* bucket_addr) {
        size_t count = 0;
        // rehash bucket bi
        auto bucket_meta = locateBucket(bi);
        uint32_t old_associate_size = bucket_meta.info.associate_size;

        // create new bucket and initialize its meta
        uint32_t new_associate_size      = old_associate_size << 1;
        uint32_t new_associate_size_mask = new_associate_size - 1;
        if (!isPowerOfTwo(new_associate_size)) {
            printf("rehash bucket is not power of 2\n");
            exit(1);
        }
        BucketMeta new_bucket_meta;
        char* new_bucket_addr = bucket_addr;
        if (new_bucket_addr == nullptr) {
            perror("rehash alloc memory fail\n");
            exit(1);
        }
        new_bucket_meta.__addr = new_bucket_addr;
        // memset(new_bucket_meta.Address(), 0, new_associate_size * kCellSize);
        new_bucket_meta.info.associate_size = new_associate_size;

        // iterator old bucket and insert slots info to new bucket
        // old: |11111111|22222222|33333333|44444444|   
        //       ========>
        // new: |1111    |22222   |333     |4444    |1111    |222     |33333   |4444    |
        std::vector<uint8_t> slot_vec(new_associate_size, CellMeta::StartSlotPos());
        BucketIterator<CellMeta> iter(bi, bucket_meta.Address(), bucket_meta.info.associate_size);
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
        return count;
    }

    ~DramHashTable() {

    }
    

    bool Put(const util::Slice& key, const util::Slice& value) {
        uint16_t log_id;
        uint32_t log_offset;
        // // step 1: put the entry to queue_, thread safe
        // // obtain current queue tail and advance queue tail atomically
        // uint32_t queue_tail = queue_tail_.fetch_add(1, std::memory_order_relaxed);
        // // add kv pair to queue
        // uint32_t queue_index = queue_tail & queue_size_mask_;
        // queue_[queue_index] = {kTypeValue, {key, value}};

        // // step 2: wait until we obtain the ownership. spinlock
        // // will wait until queue_index is equal to queue_head_.
        // // means it is my turn
        // while (queue_tail != queue_head_.load(std::memory_order_relaxed));
        // // {{{ ======= atomic process start
        // // step 3: calculate the offset in the log and release the onwership to next request. (atomic)
        // size_t record_size = Media::FormatRecordSize(kTypeValue, key, value);
        // // if log offset is larger than file size, we create a new log file
        // if (cur_log_offset_ + record_size > kMaxLogFileSize) {
        //     createNewLogFile();
        // }
        // uint16_t log_id = cur_log_id_;
        // uint32_t log_offset = cur_log_offset_;
        // cur_log_offset_ += record_size;
        // // ======= atomic process end }}}
        // // release spinlock by adding queue_head
        // queue_head_.fetch_add(1, std::memory_order_relaxed);

        // step 4: write record to log_offset
        // calculate hash value of the key
        size_t hash_value = Hasher::hash(key.data(), key.size());
        // store the kv pair to media
        void* media_offset = storeValueToMedia(key, value, log_id, log_offset);

        // step 5: update DRAM index, thread safe
        return insertSlot(key, hash_value, media_offset);
    }

    // Return the entry if key exists
    bool Get(const std::string& key, std::string* value) {
        size_t hash_value = Hasher::hash(key.data(), key.size());
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
        BucketIterator<CellMeta> iter(bucket_meta.Address(), bucket_meta.info.associate_size);
        while (iter.valid()) {
            auto res = (*iter);
            SlotInfo& info = res.first;
            info.bucket = i;
            HashSlot& slot = res.second;
            slot.meta.H1 = 0;
            auto datanode = Media::ParseData(slot.entry);
            printf("%s, addr: %16lx. key: %.8s, value: %s\n", 
                info.ToString().c_str(),
                (uint64_t)slot.entry, 
                datanode.second.first.ToString().c_str(),
                datanode.second.second.ToString().c_str());
            ++iter;
        }
    }

    void IterateAll() {
        size_t count = 0;
        for (size_t i = 0; i < bucket_count_; ++i) {
            auto bucket_meta = locateBucket(i);
            BucketIterator<CellMeta> iter(i, bucket_meta.Address(), bucket_meta.info.associate_size);
            while (iter.valid()) {
                auto res = (*iter);
                SlotInfo& info = res.first;
                HashSlot& slot = res.second;
                slot.meta.H1 = 0;
                auto datanode = Media::ParseData(slot.entry);
                printf("%s, addr: %16lx. key: %.8s, value: %s\n", 
                    info.ToString().c_str(),
                    (uint64_t)slot.entry, 
                    datanode.second.first.ToString().c_str(),
                    datanode.second.second.ToString().c_str());
                ++iter;
                count++;
            }
        }
        printf("iterato %lu entries. total size: %lu\n", count, size_);
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
        ProbeStrategy probe(0, meta.AssociateSize() - 1, bucket_i, bucket_count_);
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
    inline std::string logFileName(uint32_t log_seq) {
        char buf[128];
        sprintf(buf, "%s/%010u_%010u.ldata", path_.c_str(), log_seq, 0);
        return buf;
    }

    // create a pmem log, create a log_id->pmem_addr mapping
    inline void createNewLogFile() {
        printf("Create a new log\n");
        char* pmem_addr = nullptr;
        size_t mapped_len_;
        int is_pmem_;
        
        cur_log_offset_ = 0;
        cur_log_id_++;
        if (cur_log_id_ >= 65536) {
            cur_log_id_ = 0;
        }
        log_seq_++;

        std::string filename = logFileName(log_seq_);
        if (Media::isOptane() && (pmem_addr = (char *)pmem_map_file(filename.c_str(), kMaxLogFileSize, PMEM_FILE_CREATE, 0666, &mapped_len_, &is_pmem_)) == NULL) {
            perror("pmem_map_file");
            exit(1) ;
        }
        logid2filename_[cur_log_id_] = filename;
        logid2pmem_[cur_log_id_] = pmem_addr;
    }

    inline uint32_t bucketIndex(uint64_t hash) {
        return hash & bucket_mask_;
    }

    inline BucketMeta locateBucket(uint32_t bi) {
        return BucketMeta(cells_ + bi * associate_size_ * kCellSize, associate_size_);
        // return buckets_[bi];
    }
    // offset.first: bucket index
    // offset.second: associate index
    inline char* locateCell(const std::pair<size_t, size_t>& offset) {
        return cells_ + offset.first * associate_size_ * kCellSize + offset.second * kCellSize;
        // return  buckets_[offset.first].Address() +  // locate the bucket
        //         offset.second * kCellSize;          // locate the associate cell
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
        slot_pos->entry = media_offset;
        slot_pos->meta.H1 = info.H1;
       
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
        // atomically update the media offset to relative slot
        bool retry_find = false;
        do {
            // concurrent insertion may find the same position
            auto res = findSlotForInsert(key, hash_value);

            if (res.second) {
                // find a valid slot
                // obtain the cell lock
                char* cell_addr = locateCell({res.first.bucket, res.first.associate});
                SpinLockScope lock_scope((turbo_bitspinlock*)cell_addr);
                
                CellMeta meta(cell_addr);
                // util::PrefetchForWrite((void*)cell_addr);
                if (likely(!meta.Occupy(res.first.slot) || res.first.equal_key)) {
                    // if the slot is not occupied or the slot has same key with request
                    // we update the slot 

                    // update slot content (including pointer and H1), H2 and bitmap
                    updateSlotAndMeta(cell_addr, res.first, media_offset);
                    if (!res.first.equal_key) size_++;
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


    // Store the value to media and return the pointer to the media position
    // where the value stores
    inline void* storeValueToMedia(const Slice& key, const Slice& value, uint16_t log_id, uint32_t log_offset) {
        // void* buffer = malloc(key.size());
        // memcpy(buffer, key.data(), key.size());
        // return buffer;

        // calculate optane address
        char* pmem_addr = logid2pmem_[log_id] + log_offset;
        void* buffer = Media::Store(key, value, pmem_addr, log_id, log_offset);
        return buffer;
    }

    inline bool slotKeyEqual(const HashSlot& slot, const Slice& key) {
        // auto tmp = slot;
        // tmp.meta.H1 = 0;
        // return memcmp(tmp.entry, key.data(), key.size()) == 0;

        auto tmp = slot;
        tmp.meta.H1 = 0;
        Slice res = Media::ParseKey(tmp.entry);
        return res == key;
    }


    inline const Slice extractSlice(const HashSlot& slot, size_t len) {
        auto tmp = slot;
        tmp.meta.H1 = 0;
        return  Media::ParseKey(tmp.entry);
    }

    
    // Find a valid slot for insertion
    // Return: std::pair
    //      first: the slot info that should insert the key
    //      second: whether we can find a empty slot to insert
    // Node: Only when the second value is true, can we insert this key
    inline std::pair<SlotInfo, bool> findSlotForInsert(const Slice& key, size_t hash_value) {
        PartialHash partial_hash(hash_value);
        uint32_t bucket_i = bucketIndex(partial_hash.bucket_hash_);
        // BucketMeta bucket_meta = locateBucket(bucket_i);
        ProbeStrategy probe(partial_hash.H1_, associate_size_ - 1 /*bucket_meta.AssociateSize() - 1*/, bucket_i, bucket_count_);

        // limit probe count
        int probe_count = 0;
        while (probe && probe_count++ < ProbeStrategy::MAX_PROBE_LEN) {
            auto offset = probe.offset();
            char* cell_addr = locateCell(offset);
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
                        return {{offset.first, offset.second, i, partial_hash.H1_, partial_hash.H2_, true}, true};
                    }
                    else {
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
            auto empty_bitset = meta.EmptyBitSet(); 
            if (likely(empty_bitset)) {
                // there is empty slot for insertion, so we return a slot
                for (int i : empty_bitset) {
                    return {{offset.first, offset.second, i, partial_hash.H1_, partial_hash.H2_, false}, true};
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
            util::PrefetchForRead(slot.entry);
        }
    }

    
    inline std::pair<SlotInfo, bool> findSlot(const Slice& key, size_t hash_value) {
        PartialHash partial_hash(hash_value);
        uint32_t bucket_i = bucketIndex(partial_hash.bucket_hash_);
        // BucketMeta bucket_meta = locateBucket(bucket_i);
        ProbeStrategy probe(partial_hash.H1_, associate_size_ - 1 /* bucket_meta.AssociateSize() - 1*/, bucket_i, bucket_count_);

        // limit probe count
        int probe_count = 0;
        while (probe && probe_count++ < ProbeStrategy::MAX_PROBE_LEN) {
            auto offset = probe.offset();
            char* cell_addr = locateCell(offset);
            CellMeta meta(cell_addr);
            // locate if there is any H2 match in this cell
            for (int i : meta.MatchBitSet(partial_hash.H2_)) {
                // i is the slot index in current cell, each slot occupies 8-byte

                // locate the slot reference
                const HashSlot& slot = *locateSlot(cell_addr, i);

                // compare if the H1 partial hash is equal
                if (likely(slot.meta.H1 == partial_hash.H1_)) {
                    // compare the actual key pointed by the pointer 
                    // with the parameter key
                    if (likely(slotKeyEqual(slot, key))) {
                        // slotKeyEqual is very expensive 
                        return {{offset.first, offset.second, i, partial_hash.H1_, partial_hash.H2_, true} , true};
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
    std::string   path_ = "./";         // default path for pmem
    char*         cells_ = nullptr;
    const size_t  bucket_count_ = 0;
    const size_t  bucket_mask_  = 0;
    size_t        associate_size_ = 0;
    size_t        associate_mask_  = 0;
    size_t        capacity_ = 0;
    size_t        size_;

    // ----- circular queue for synchronizing put requests -----
    std::vector<std::pair<char, std::pair<Slice, Slice> > > queue_; // queue that store request sequence. (type, kv entry)
    uint32_t              queue_size_mask_;
    std::atomic<uint32_t> queue_tail_;
    std::atomic<uint32_t> queue_head_;
    uint32_t              queue_head_for_log_;  // used for background logging thread
 
    std::string logid2filename_[65536]; // log_id -> pmem file: After rebooting, this should be loaded from persistent memory,
                                                                    //                      then logid2pmem_ is updated.
    // {{{ guarded by spinlock (while loop)
    uint64_t    log_seq_ = 0;
    uint16_t    cur_log_id_ = 0;
    uint32_t    cur_log_offset_ = 0;    // logid2pmem_[cur_log_id_] + cur_log_offset_ : actual address in pmem
    char*       logid2pmem_[65536];     // map that stores the pmem file initial address, should be initialized when rebooting. log_id_ -> pmem_addr
    // }}}


    const int       kCellSize = CellMeta::CellSize();
    const int       kCellSizeLeftShift = CellMeta::CellSizeLeftShift;
    const size_t    kMaxLogFileSize = 4LU << 30;        // 4 GB

    BucketMeta*     buckets_;
};

}