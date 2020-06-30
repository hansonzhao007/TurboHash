#pragma once
#include <string>

#include "util/slice.h"
#include "util/status.h"
#include "util/coding.h"
#include "hash_function.h"

namespace turbo {
using Status = util::Status;
using Slice = util::Slice;

union HashSlot
{
    /* data */
    const void* entry;       // 8B
    struct {
        uint32_t offset;
        uint16_t log_id;
        uint16_t H1;
    } meta;
};

// 64 bit hash function is used to locate initial position of keys
// bucket_hash_: index within each bucket
// H2: 1 byte hash for parallel comparison
// H1: 2 byte partial key
#define BUCKET_H_SIZE uint32_t
#define LTHASH_H3_SIZE uint8_t
#define LTHASH_H2_SIZE uint8_t 
#define LTHASH_H1_SIZE uint16_t
struct PartialHash {
    PartialHash(uint64_t hash)
        :
        bucket_hash_(hash >> 32),
        H1_((hash & 0xFFFF) ^ (hash >> 16)),
        H2_((hash >> 16) & 0xFF)
        { };
    BUCKET_H_SIZE  bucket_hash_;
    // H1: 2 byte partial key
    LTHASH_H1_SIZE H1_;
    // H2: 1 byte hash for parallel comparison
    LTHASH_H2_SIZE H2_;
};

class SlotInfo {
public:
    uint32_t bucket;        // bucket index
    uint16_t associate;     // associate index
    uint8_t  slot;          // slot index
    LTHASH_H2_SIZE H2;
    LTHASH_H1_SIZE H1;
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
    std::string ToString() {
        char buffer[128];
        sprintf(buffer, "b: %4u, a: %4u, s: %2u, H2: 0x%02x, H1: 0x%04x",
            bucket,
            associate,
            slot,
            H2,
            H1);
        return buffer;
    }
};
    
// bits 0 - 6 are uesd as magic number to indicate the start of a record
enum ValueType {
    kTypeDeletion = 0x2A,   // 0b 0_0101010
    kTypeValue    = 0xAA,   // 0b 1_0101010
    kTypeMask     = 0x7F    // 0b 0_1111111
};

class BucketMeta {
public:
    BucketMeta(char* addr, uint16_t associate_size):
        __addr(addr)
        {
            info.associate_size = associate_size;
    }
    BucketMeta(){}
    inline char* Address() {
        return (char*)(uint64_t(__addr) & 0x0000FFFFFFFFFFFF);
    }
    inline uint16_t AssociateSize() {
        return info.associate_size;
    }
    union {
        char* __addr;
        struct {
            uint32_t none0;
            uint16_t none1;
            uint16_t associate_size;
        } info;
    };
};


// class Handler {
// public: 
//     virtual void Run(ValueType, const Slice& key, const Slice& value) = 0;
// };

// // records :=
// //    kTypeValue varstring varstring 
// //    kTypeDeletion varstring
// // varstring :=
// //    len: varint32
// //    data: uint8[len]
// Status IterateRecords(const std::string& records, Handler* handler, size_t* records_count) {
//     Slice input(records);
//     Slice key, value;
//     int found = 0;
//     while (!input.empty()) {
//         found++;
//         unsigned char tag = input[0];
//         input.remove_prefix(1);
//         switch (tag) {
//         case kTypeValue:
//             if (GetLengthPrefixedSlice(&input, &key) &&
//                 GetLengthPrefixedSlice(&input, &value)) {
//                 handler->Run(kTypeValue, key, value);
//             } else {
//                 return Status::Corruption("bad kTypeValue");
//             }
//             break;
//         case kTypeDeletion:
//             if (GetLengthPrefixedSlice(&input, &key)) {
//                 handler->Run(kTypeDeletion, key, value);
//             } else {
//                 return Status::Corruption("bad kTypeDeletion");
//             }
//             break;
//         default:
//             return Status::Corruption("unknown ValueType");
//         }
//     } 

//     *records_count = found;
//     return Status::OK();
// }

// inline
// void SerilizeRecord(ValueType type, const Slice& key, const Slice& value, std::string* output) {
//     output->push_back(static_cast<char>(type));
//     PutLengthPrefixedSlice(output, key);
//     if (type != kTypeDeletion) PutLengthPrefixedSlice(output, value);
// }


// // Note: the rep_ will be stored to media
// // WriteBatch::rep_ :=
// //    data: record[count]
// // record :=
// //    kTypeValue varstring varstring         |
// //    kTypeDeletion varstring
// // varstring :=
// //    len: varint32
// //    data: uint8[len]
// class WriteBatch {
// public:
//     inline void Put(const Slice& key, const Slice& value) {
//         count_++;
//         rep_.push_back(static_cast<char>(kTypeValue));
//         PutLengthPrefixedSlice(&rep_, key);
//         PutLengthPrefixedSlice(&rep_, value);
//     }

//     inline void Delete(const Slice& key) {
//         count_++;
//         rep_.push_back(static_cast<char>(kTypeDeletion));
//         PutLengthPrefixedSlice(&rep_, key);
//     }

//     inline size_t Count() {
//         return count_;
//     }

//     inline size_t Size() {
//         return rep_.size();
//     }

// private:
//     size_t count_ = 0;
//     std::string rep_;
// };

}