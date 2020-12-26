#include "turbo/turbo_hash.h"
#include "util/env.h"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;


int main() {
    turbo::unordered_map<std::string, std::string> map;

    const size_t COUNT = 100000;
    auto* hashtable = new turbo::unordered_map<std::string, std::string> (8, 2048);
    printf("------- Iterate empty hash table ------\n");
    hashtable->IterateAll();

    bool succ = true;
    size_t find = 0;
    for (size_t i = 0 ; i < COUNT && succ; i++) {
        succ = hashtable->Put("key" + std::to_string(i), "value" + std::to_string(i));
        if ((succ)) find++;
    }
    printf("inserted %lu kv, hashtable size: %lu, loadfactor: %f\n", find, hashtable->Size(), hashtable->LoadFactor());

    find = 0;
    for (size_t i = 0 ; i < 100 && succ; i++) {
        succ = hashtable->Put("key" + std::to_string(i), "update.value" + std::to_string(i));
        if ((succ)) find++;
    }
    printf("update %lu kv, hashtable size: %lu, loadfactor: %f\n", find, hashtable->Size(), hashtable->LoadFactor());

    auto read_fun = [&hashtable] {
        bool succ = true;
        std::string value;
        size_t find = 0;
        for (size_t i = 0; i < COUNT && succ; i++) {
            std::string key = "key" + std::to_string(i);
            succ = hashtable->Get(key, &value);
            if (i < 100 || (i & 0x7FF) == 0) {
                INFO("Get key: %s. value: %s\n", key.c_str(), value.c_str());
            }
            if ((succ)) find++;
        }
        printf("find %lu key, hashtable size: %lu, loadfactor: %f\n", find, hashtable->Size(), hashtable->LoadFactor());
    };
    read_fun();
    // printf("------- Iterate hash table with %lu entries ------\n", hashtable->Size());
    // hashtable->IterateBucket(1);
    // hashtable->PrintAllMeta();
    // hashtable->IterateAll();
    // hashtable->IterateValidBucket();
    
    printf("------- rehash all bucket and repeat search ------\n");
    hashtable->MinorReHashAll();
    // hashtable->IterateValidBucket();
    // hashtable->IterateBucket(1);
    // printf("%s\n", hashtable->PrintBucketMeta(1).c_str());
    // hashtable->IterateAll();
    read_fun();

    hashtable->MinorReHashAll();
    read_fun();

    hashtable->MinorReHashAll();
    read_fun();

    hashtable->MinorReHashAll();
    read_fun();

    {
        typedef turbo::unordered_map<int, double> MyHash;
        MyHash mapi;
        MyHash::HashSlot slot;
        decltype(slot.entry) entry;
        decltype(slot.H1) h1;
        MyHash::H1Tag a;
        INFO("HashSlot size: %lu\n", sizeof(MyHash::HashSlot));
        for (int i = 0; i < 100; i++) {
            mapi.Put(i, i * 1.0);
        }

        for (int i = 0; i < 100; i++) {
            auto res = mapi.Find(i);
            if (res == nullptr) {
                printf("Fail get\n");
            }
            INFO("Get integer key: %d, val: %f\n", res->first(), res->second());
        }
    }

    {
        typedef turbo::unordered_map<double, std::string> MyHash;
        MyHash mapi(2, 32);
        MyHash::HashSlot slot;
        decltype(slot.entry) entry;
        decltype(slot.H1) h1;
        MyHash::H1Tag a;
        INFO("HashSlot size: %lu\n", sizeof(MyHash::HashSlot));
        for (int i = 0; i < 100; i++) {
            mapi.Put(i * 1.01, "value" + std::to_string(i));
        }

        for (int i = 0; i < 100; i++) {
            auto res = mapi.Find(i * 1.01);
            if (res == nullptr) {
                printf("Fail get\n");
            }
            INFO("Get double key: %f, val: %s\n", res->first(), res->second().ToString().c_str());
        }
        // std::cout << mapi.PrintBucketMeta(1) << std::endl;
    }

    {
        typedef turbo::unordered_map<std::string, double> MyHash;
        MyHash mapi;
        MyHash::HashSlot slot;
        decltype(slot.entry) entry;
        decltype(slot.H1) h1;
        MyHash::H1Tag a;
        INFO("HashSlot size: %lu\n", sizeof(MyHash::HashSlot));
        for (int i = 0; i < 100; i++) {
            mapi.Put("key" + std::to_string(i), i);
        }

        for (int i = 0; i < 100; i++) {            
            auto res = mapi.Find("key" + std::to_string(i));
            if (res == nullptr) {
                printf("Fail get\n");
            }
            INFO("Get str key: %s, val: %f\n", res->first().ToString().c_str(), res->second() );
        }
    }

    {
        typedef turbo::unordered_map<int, int> MyHash;
        MyHash mapi(8, 16);
        MyHash::HashSlot slot;
        decltype(slot.entry) entry;
        decltype(slot.H1) h1;
        MyHash::H1Tag a;
        INFO("HashSlot size: %lu\n", sizeof(MyHash::HashSlot));
        for (int i = 0; i < 100; i++) {
            mapi.Put(i, i);
        }

        for (int i = 0; i < 100; i++) {            
            auto res = mapi.Find(i);
            if (res == nullptr) {
                printf("Fail get\n");
            }
            INFO("Get int key: %d, int: %d\n", res->first(), res->second() );
        }

        mapi.Delete(20);
        if (mapi.Find(20) != nullptr) {
            printf("Cannot delete key");
        }

        mapi.PrintAllMeta();
    }

    return 0;
}