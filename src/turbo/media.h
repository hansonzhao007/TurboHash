#pragma once

#include "util/slice.h"
#include "format.h"
namespace turbo {

/** 
 *  Dram Record Format: 
 *  | ValueType | key size | value size |   key   |   value   |
 *  |    1B     |   4B     |    4B      |   ....
*/
class DramMedia {
public:
    static inline bool isOptane() { return false;}
    static inline size_t FormatRecordSize(ValueType type, const util::Slice& key, const util::Slice& value) {
        if (type == kTypeValue) 
            return key.size() + value.size() + 9;
        else if (type == kTypeDeletion) {
            return key.size() + 5;
        }
    }

    static inline void* Store(const util::Slice& key, const util::Slice& value, char* addr, uint16_t log_id, uint32_t log_offse) {
        ValueType type = kTypeValue;
        size_t key_len = key.size();
        size_t value_len = value.size();
        char* buffer = (char*)malloc(FormatRecordSize(type, key, value));
        // store value type
        memcpy(buffer, &type, 1);
        // store key len
        memcpy(buffer + 1, &key_len, 4);
        // store key
        memcpy(buffer + 5, key.data(), key_len);
         // store value len
        memcpy(buffer + 5 + key_len, &value_len, 4);
        // store value
        memcpy(buffer + 9 + key_len, value.data(), value_len);
        return buffer;
    }

    static inline void* Delete(const util::Slice& key, char* addr) {
        ValueType type = kTypeValue;
        size_t key_len = key.size();
        char* buffer = (char*)malloc(FormatRecordSize(type, key, ""));
        // store value type
        memcpy(buffer, &type, 1);
        // store key len
        memcpy(buffer + 1, &key_len, 4);
        // store key
        memcpy(buffer + 5, key.data(), key_len);
        return buffer;
    }

    static inline util::Slice ParseKey(const void* _addr) {
        char* addr = (char*) _addr;
        uint32_t key_len = 0;
        memcpy(&key_len, addr + 1, 4);
        return util::Slice(addr + 5, key_len);
    }


    static inline std::pair<ValueType, std::pair<util::Slice, util::Slice> > ParseData(const void* _addr) {
        char* addr = (char*) _addr;
        ValueType type = kTypeValue;
        uint32_t key_len = 0;
        uint32_t value_len = 0;
        memcpy(&type, addr, 1);
        memcpy(&key_len, addr + 1, 4);
        if (type == kTypeValue) {
            memcpy(&value_len, addr + 5 + key_len, 4);
            return {type, {util::Slice(addr + 5, key_len), util::Slice(addr + 9 + key_len, value_len)} };
        } else if (type == kTypeDeletion) {
            return {type, {util::Slice(addr + 5, key_len), ""} };
        } else {
            printf("Prase type incorrect: %d\n", type);
            exit(1);
        }
        
    }
};


}