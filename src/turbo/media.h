#pragma once


#ifdef JEMALLOC
    #include <jemalloc/jemalloc.h>
#endif


#include "util/slice.h"
#include "format.h"
namespace turbo {

struct Record {
    ValueType type;
    Slice key;
    Slice value;
};

/** 
 *  Dram Record Format: 
 *  | ValueType | key size | key | value size |  value  |
 *  |    1B     |   4B     | ... |    4B      |   ...
*/
class DramMedia {
public:

    static inline void* Store(ValueType type, const util::Slice& key, const util::Slice& value) {
        size_t key_len = key.size();
        size_t value_len = value.size();
        size_t encode_len = key_len + value_len;
        if (type == kTypeValue) {
            // has both key and value
            encode_len += 9;
        } else if (type == kTypeDeletion) {
            encode_len += 5;
        }

        char* buffer = (char*)malloc(encode_len);
        
        // store value type
        memcpy(buffer, &type, 1);
        // store key len
        memcpy(buffer + 1, &key_len, 4);
        // store key
        memcpy(buffer + 5, key.data(), key_len);

        if (type == kTypeDeletion) {
            return buffer;
        }

         // store value len
        memcpy(buffer + 5 + key_len, &value_len, 4);
        // store value
        memcpy(buffer + 9 + key_len, value.data(), value_len);
        return buffer;
    }


    static inline util::Slice ParseKey(const void* _addr) {
        char* addr = (char*) _addr;
        uint32_t key_len = 0;
        memcpy(&key_len, addr + 1, 4);
        return util::Slice(addr + 5, key_len);
    }


    static inline Record ParseData(uint64_t offset) {
        char* addr = (char*) offset;
        ValueType type = kTypeValue;
        uint32_t key_len = 0;
        uint32_t value_len = 0;
        memcpy(&type, addr, 1);
        memcpy(&key_len, addr + 1, 4);
        if (type == kTypeValue) {
            memcpy(&value_len, addr + 5 + key_len, 4);
            return {
                type, 
                util::Slice(addr + 5, key_len), 
                util::Slice(addr + 9 + key_len, value_len)};
        } else if (type == kTypeDeletion) {
            return {
                type, 
                util::Slice(addr + 5, key_len), 
                "" };
        } else {
            printf("Prase type incorrect: %d\n", type);
            exit(1);
        }
    }

};


}