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
    for (size_t i = 0 ; i < 1000 && succ; i++) {
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
            if ((i & 0xFF) == 0) {
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
    hashtable->DebugInfo();
    read_fun();

    hashtable->MinorReHashAll();
    hashtable->DebugInfo();
    read_fun();

    hashtable->MinorReHashAll();
    hashtable->DebugInfo();
    read_fun();

    {
        turbo::unordered_map<int, double> mapi;
        for (int i = 0; i < 100; i++) {
            mapi.Put(i, i *3.1415926);
        }

        for (int i = 0; i < 100; i++) {
            double val = 0;
            bool res = mapi.Get(i, &val);
            if (res == false) {
                printf("Fail get\n");
            }
            INFO("Get integer key: %d, val: %f\n", i, val);
        }
    }

    {
        turbo::unordered_map<double, std::string> mapi;
        for (int i = 0; i < 100; i++) {
            mapi.Put(i * 3.14, "value" + std::to_string(i));
        }

        for (int i = 0; i < 100; i++) {
            std::string val;
            bool res = mapi.Get(i * 3.14, &val);
            if (res == false) {
                printf("Fail get\n");
            }
            INFO("Get double key: %f, val: %s\n", i * 3.14, val.c_str());
        }
    }

    {
        turbo::unordered_map<std::string, double> mapi;
        for (int i = 0; i < 100; i++) {
            mapi.Put("key" + std::to_string(i), 3.14 * i);
        }

        for (int i = 0; i < 100; i++) {
            double val;
            bool res = mapi.Get("key" + std::to_string(i), &val);
            if (res == false) {
                printf("Fail get\n");
            }
            INFO("Get val: %f\n", val);
        }
    }

    return 0;
}