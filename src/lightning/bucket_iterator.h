#pragma once
#include "bitset.h"

namespace lthash {

/** Usage: iterator every slot in the bucket, return the pointer in the slot
 *  BucketIterator<CellMeta> iter(bucket_addr, associate_count_);
 *  while (iter != iter.end() && iter.valid()) {
 *      ...
 *      ++iter;
 *  }
*/        
template<class CellMeta>
class BucketIterator {
public:
    BucketIterator(char* bucket_addr, size_t associate_size, size_t assocaite_i = 0):
        associate_size_(associate_size),
        associate_i_(assocaite_i),
        bitmap_(0),
        bucket_addr_(bucket_addr) {
        if (bucket_addr != nullptr) {
            CellMeta meta(bucket_addr);
            bitmap_ = meta.OccupyBitSet();
        }
        toFirstValid();
    }

    explicit operator bool() const {
        return (associate_i_ < associate_size_) ||
               (associate_size_ == associate_size_ && bitmap_);
    }

    // ++iterator
    BucketIterator& operator++() {
        ++bitmap_;
        if (!bitmap_) {
            associate_i_++;
            if (associate_i_ < associate_size_) {
                char* cell_addr = bucket_addr_ + associate_i_ * CellMeta::CellSize();
                CellMeta meta(cell_addr);
                bitmap_ = meta.OccupyBitSet();
            }
            else {
                bitmap_ = BitSet();
            }
            
        }
        return *this;
    }

    char* operator*() const {
        return  bucket_addr_ + associate_i_ * CellMeta::CellSize() + *bitmap_ * 8;
    }

    bool valid() {
        return bitmap_ ? true : false;
    }
    BucketIterator begin() const {
        return *this;
    }

    BucketIterator end() const {
        return BucketIterator(nullptr, associate_size_, associate_size_);
    }

    std::string ToString() {
        char buffer[128];
        sprintf(buffer, "associate: %8d, slot: %2d", associate_i_, *bitmap_);
        return buffer;
    }
private:
    void toFirstValid() {
        while(!bitmap_ && associate_i_ < associate_size_) {
            associate_i_++;
            char* cell_addr = bucket_addr_ + associate_i_ * CellMeta::CellSize();
            CellMeta meta(cell_addr);
            bitmap_ = meta.OccupyBitSet();
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
    uint32_t    associate_size_;
    uint32_t    associate_i_; 
    BitSet      bitmap_;
    char*       bucket_addr_;

};

}