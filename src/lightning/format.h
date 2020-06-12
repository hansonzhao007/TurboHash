#pragma once
#include <string>

#include "util/slice.h"
#include "util/status.h"
#include "util/coding.h"

namespace lthash {
using Status = util::Status;
using Slice = util::Slice;

// bits 0 - 6 are uesd as magic number to indicate the start of a record
enum ValueType {
    kTypeDeletion = 0x2A,   // 0b 0_0101010
    kTypeValue    = 0xAA,   // 0b 1_0101010
    kTypeMask     = 0x7F    // 0b 0_1111111
};


class Handler {
public: 
    virtual void Run(ValueType, const Slice& key, const Slice& value) = 0;
};

// records :=
//    kTypeValue varstring varstring 
//    kTypeDeletion varstring
// varstring :=
//    len: varint32
//    data: uint8[len]
Status IterateRecords(const std::string& records, Handler* handler, size_t* records_count) {
    Slice input(records);
    Slice key, value;
    int found = 0;
    while (!input.empty()) {
        found++;
        unsigned char tag = input[0];
        input.remove_prefix(1);
        switch (tag) {
        case kTypeValue:
            if (GetLengthPrefixedSlice(&input, &key) &&
                GetLengthPrefixedSlice(&input, &value)) {
                handler->Run(kTypeValue, key, value);
            } else {
                return Status::Corruption("bad kTypeValue");
            }
            break;
        case kTypeDeletion:
            if (GetLengthPrefixedSlice(&input, &key)) {
                handler->Run(kTypeDeletion, key, value);
            } else {
                return Status::Corruption("bad kTypeDeletion");
            }
            break;
        default:
            return Status::Corruption("unknown ValueType");
        }
    } 

    *records_count = found;
    return Status::OK();
}

inline
void SerilizeRecord(ValueType type, const Slice& key, const Slice& value, std::string* output) {
    output->push_back(static_cast<char>(type));
    PutLengthPrefixedSlice(output, key);
    if (type != kTypeDeletion) PutLengthPrefixedSlice(output, value);
}


// Note: the rep_ will be stored to media
// WriteBatch::rep_ :=
//    data: record[count]
// record :=
//    kTypeValue varstring varstring         |
//    kTypeDeletion varstring
// varstring :=
//    len: varint32
//    data: uint8[len]
class WriteBatch {
public:
    inline void Put(const Slice& key, const Slice& value) {
        count_++;
        rep_.push_back(static_cast<char>(kTypeValue));
        PutLengthPrefixedSlice(&rep_, key);
        PutLengthPrefixedSlice(&rep_, value);
    }

    inline void Delete(const Slice& key) {
        count_++;
        rep_.push_back(static_cast<char>(kTypeDeletion));
        PutLengthPrefixedSlice(&rep_, key);
    }

    inline size_t Count() {
        return count_;
    }

    inline size_t Size() {
        return rep_.size();
    }

private:
    size_t count_ = 0;
    std::string rep_;
};

}