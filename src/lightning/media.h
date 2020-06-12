#pragma once

#include "util/slice.h"



namespace lthash {


class Media {
public:
    virtual void* Store(const util::Slice& key, const util::Slice& value, char* addr) = 0;
    virtual std::pair<util::Slice, util::Slice> Parse(const char* addr) = 0;
};


/** 
 *  Dram Record Format: 
 *  | key size | value size |   key   |   value   |
 *  |   4B     |    4B      |   ....
*/
class DramMedia: public Media {
public:
    void* Store(const util::Slice& key, const util::Slice& value, char* addr) override {
        uint32_t key_len = key.size();
        uint32_t value_len = value.size();
        char* buffer = (char*)malloc(key.size() + value.size() + sizeof(int) * 2);
        // store key len
        memcpy(buffer, &key_len, 4);
        // store value len
        memcpy(buffer + 4, &value_len, 4);
        // store key
        memcpy(buffer + 8, key.data(), key_len);
        // store value
        memcpy(buffer + 8 + key_len, value.data(), value_len);
        return buffer;
    }

    std::pair<util::Slice, util::Slice> Parse(const char* addr) override {
        uint32_t key_len = 0;
        uint32_t value_len = 0;
        memcpy(&key_len, addr, 4);
        memcpy(&value_len, addr + 4, 4);
        return {util::Slice(addr + 8, key_len), util::Slice(addr + 8 + key_len, value_len)};
    }
};


}