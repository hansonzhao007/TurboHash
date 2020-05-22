#pragma once

#include "util/slice.h"



namespace lthash {


/** 
 *  Format: 
 *  | key size | value size |   key   |   value   |
 *  |   4B     |    4B      |   ....
*/
class Media {
public:
    virtual void Store(const util::Slice& key, const util::Slice& value) = 0;
    virtual std::pair<util::Slice, util::Slice> Parse(const void* addr) = 0;
};

class DramMedia: public Media {
public:
    void Store(const util::Slice& key, const util::Slice& value) override {

    }

    std::pair<util::Slice, util::Slice> Parse(const void* addr) override {

    }
};


}