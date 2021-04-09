#include "turbo/turbo_hash.h"
#include "util/logger.h"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

#ifdef TURBO_HASH_H_
using HashTable = turbo::unordered_map<std::string, std::string>;
#define hashnamespace turbo
#elif defined TURBO_HASH_PMEM_H_
using HashTable = turbo_pmem::unordered_map<std::string, std::string>;
#define hashnamespace turbo_pmem
#endif

int main() {
    const size_t COUNT = 100000;

    
    auto* hashtable = new HashTable(8, 128);
   
    
    printf("------- Iterate empty hash table ------\n");
    printf("hashtable size: %lu, capacity: %lu, loadfactor: %f\n", hashtable->Size(), hashtable->Capacity(), hashtable->LoadFactor());
    hashtable->IterateAll();

    bool succ = true;
    size_t find = 0;
    for (size_t i = 0 ; i < COUNT && succ; i++) {
        succ = hashtable->Put("key" + std::to_string(i), "value" + std::to_string(i));
        if ((succ)) find++;
    }
    printf("inserted %lu kv, hashtable size: %lu, capacity: %lu, loadfactor: %f\n", find, hashtable->Size(), hashtable->Capacity(), hashtable->LoadFactor());

    find = 0;
    for (size_t i = 0 ; i < 100 && succ; i++) {
        succ = hashtable->Put("key" + std::to_string(i), "updat" + std::to_string(i));
        if ((succ)) find++;
    }
    printf("inserted %lu kv, hashtable size: %lu, capacity: %lu, loadfactor: %f\n", find, hashtable->Size(), hashtable->Capacity(), hashtable->LoadFactor());

    auto read_fun = [&hashtable] {
        size_t find = 0;
        for (size_t i = 0; i < COUNT; i++) {
            std::string key = "key" + std::to_string(i);
            auto res = hashtable->Find(key);
            if (i < 10 || (i & 0x7FF) == 0) {
                INFO("Get key: %s. value: %s\n", key.c_str(), res->second().c_str());
            }
            if ((res != nullptr)) find++;
        }
        printf("find %lu kv, hashtable size: %lu, capacity: %lu, loadfactor: %f\n", find, hashtable->Size(), hashtable->Capacity(), hashtable->LoadFactor());
    };
    read_fun();
    // printf("------- Iterate hash table with %lu entries ------\n", hashtable->Size());
    // hashtable->IterateBucket(1);
    // hashtable->PrintAllMeta();
    // hashtable->IterateAll();
    // hashtable->IterateValidBucket();
    
    // printf("------- rehash all bucket and repeat search ------\n");
    hashtable->MinorReHashAll();
    read_fun();

    hashtable->MinorReHashAll();
    read_fun();

    {
        typedef hashnamespace::unordered_map<int, double> MyHash;
        MyHash mapi(2, 32);
        // MyHash::HashSlot slot;
        // decltype(slot.entry) entry;
        // decltype(slot.H1) h1;
        // MyHash::H1Tag a;
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
        typedef hashnamespace::unordered_map<double, std::string> MyHash;
        MyHash mapi(2, 32);
        INFO("HashSlot size: %lu\n", sizeof(MyHash::HashSlot));
        for (int i = 0; i < 100; i++) {
            mapi.Put(i * 1.01, "value" + std::to_string(i));
        }

        for (int i = 0; i < 100; i++) {
            auto res = mapi.Find(i * 1.01);
            if (res == nullptr) {
                printf("Fail get\n");
            }
            INFO("Get double key: %f, val: %s\n", res->first(), res->second().c_str());
        }
        // std::cout << mapi.PrintBucketMeta(1) << std::endl;
    }

    {
        typedef hashnamespace::unordered_map<std::string, double> MyHash;
        MyHash mapi(2, 32);
        INFO("HashSlot size: %lu\n", sizeof(MyHash::HashSlot));
        for (int i = 0; i < 100; i++) {
            mapi.Put("key" + std::to_string(i), i);
        }

        for (int i = 0; i < 100; i++) {            
            auto res = mapi.Find("key" + std::to_string(i));
            if (res == nullptr) {
                printf("Fail get\n");
            }
            INFO("Get str key: %s, val: %f\n", res->first().c_str(), res->second() );
        }
    }

    {
        typedef hashnamespace::unordered_map<int, int> MyHash;
        MyHash mapi(2, 16);
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

        mapi.Put(20, 202);
        mapi.PrintAllMeta();       
    }

    return 0;
}