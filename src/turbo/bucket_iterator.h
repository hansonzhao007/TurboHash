#pragma once
#include "bitset.h"
#include "format.h"
namespace turbo {

/** Usage: iterator every slot in the bucket, return the pointer in the slot
 *  BucketIterator<CellMeta> iter(bucket_addr, associate_count_);
 *  while (iter.valid()) {
 *      ...
 *      ++iter;
 *  }
*/        
template<class CellMeta>
class BucketIterator {
public:
typedef std::pair<SlotInfo, HashSlot> value_type;
    BucketIterator(uint32_t bi, char* bucket_addr, size_t associate_size, size_t assocaite_i = 0):
        bi_(bi),
        associate_size_(associate_size),
        associate_i_(assocaite_i),
        bitmap_(0),
        bucket_addr_(bucket_addr) {

        assert(bucket_addr != 0);
        CellMeta meta(bucket_addr);
        bitmap_ = meta.OccupyBitSet();
        if(!bitmap_) toNextValidBitMap();
        // printf("Initial Bucket iter at ai: %u, si: %u\n", associate_i_, *bitmap_);
    }

    explicit operator bool() const {
        return (associate_i_ < associate_size_) ||
               (associate_size_ == associate_size_ && bitmap_);
    }

    // ++iterator
    inline BucketIterator& operator++() {
        ++bitmap_;
        if (!bitmap_) {
            toNextValidBitMap();
        }
        return *this;
    }

    value_type operator*() const {
        // return the associate index, slot index and its slot content
        uint8_t slot_index = *bitmap_;
        char* cell_addr = bucket_addr_ + associate_i_ * CellMeta::CellSize();
        HashSlot* slot = (HashSlot*)(cell_addr + slot_index * 8);
        uint8_t H2 = *(uint8_t*)(cell_addr + slot_index);
        return  { {bi_ /* ignore bucket index */, associate_i_ /* associate index */, *bitmap_ /* slot index*/, (uint16_t)slot->meta.H1, H2, false}, 
                *slot};
    }

    inline bool valid() {
        return associate_i_ < associate_size_ && (bitmap_ ? true : false);
    }

    std::string ToString() {
        char buffer[128];
        sprintf(buffer, "associate: %8d, slot: %2d", associate_i_, *bitmap_);
        return buffer;
    }
private:
    inline void toNextValidBitMap() {
        while(!bitmap_ && associate_i_ < associate_size_) {
            associate_i_++;
            char* cell_addr = bucket_addr_ + ( associate_i_ << CellMeta::CellSizeLeftShift );
            bitmap_ = BitSet(*(uint32_t*)(cell_addr) & CellMeta::BitMapMask); 
        }
    }
    friend bool operator==(const BucketIterator& a, const BucketIterator& b) {
        return  a.associate_i_ == b.associate_i_ &&
                a.bitmap_ == b.bitmap_;
                
    }
    friend bool operator!=(const BucketIterator& a, const BucketIterator& b) {
        return  a.associate_i_ != b.associate_i_ ||
                a.bitmap_ != b.bitmap_;
    }
    uint32_t    bi_;
    uint32_t    associate_size_;
    uint32_t    associate_i_; 
    BitSet      bitmap_;
    char*       bucket_addr_;
};

}