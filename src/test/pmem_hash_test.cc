#include "gflags/gflags.h"
#include "turbo/turbo_hash_pmem.h"
#include "util/logger.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

int main () {
    remove ("/mnt/pmem/turbo_hash_pmem_basemd");
    remove ("/mnt/pmem/turbo_hash_pmem_desc");
    remove ("/mnt/pmem/turbo_hash_pmem_sb");

    {
        const size_t COUNT = 100000;
        using HashTable = turbo_pmem::unordered_map<std::string, std::string>;
        auto* hashtable = new HashTable;
        hashtable->Initialize (8, 128);

        printf ("------- Iterate empty hash table ------\n");
        hashtable->IterateAll ();

        auto thread_info = hashtable->getThreadInfo ();

        bool succ = true;
        size_t find = 0;
        for (size_t i = 0; i < COUNT && succ; i++) {
            succ = hashtable->Put ("key" + std::to_string (i), "value" + std::to_string (i),
                                   thread_info);
            if ((succ)) find++;
        }
        printf ("inserted %lu kv\n", find);

        find = 0;
        for (size_t i = 0; i < 100 && succ; i++) {
            succ = hashtable->Put ("key" + std::to_string (i), "updat" + std::to_string (i),
                                   thread_info);
            if ((succ)) find++;
        }
        printf ("update %lu kv\n", find);

        auto read_fun = [&hashtable] {
            size_t find = 0;
            std::string value_buffer;
            auto read_callback = [&] (HashTable::RecordType record) {
                value_buffer = record.value ();
            };
            auto tinfo = hashtable->getThreadInfo ();
            for (size_t i = 0; i < COUNT; i++) {
                std::string key = "key" + std::to_string (i);
                auto res = hashtable->Find (key, tinfo, read_callback);
                if (i < 10 || (i & 0x7FF) == 0) {
                    INFO ("Get key: %s. val: %s\n", key.c_str (), value_buffer.c_str ());
                }
                if ((res)) find++;
            }
            printf ("find %lu key\n", find);
        };
        read_fun ();

        // printf("------- rehash all bucket and repeat search ------\n");
        hashtable->MinorReHashAll ();
        read_fun ();

        hashtable->MinorReHashAll ();
        read_fun ();
    }

    {
        typedef turbo_pmem::unordered_map<int, double> MyHash;
        MyHash mapi;
        mapi.Initialize (2, 32);
        auto thread_info = mapi.getThreadInfo ();
        // MyHash::HashSlot slot;
        // decltype(slot.entry) entry;
        // decltype(slot.H1) h1;
        // MyHash::H1Tag a;
        INFO ("HashSlot size: %lu\n", sizeof (MyHash::HashSlot));
        for (int i = 0; i < 100; i++) {
            mapi.Put (i, i * 1.0, thread_info);
        }

        int key_buf;
        double val_buf;
        auto read_callback = [&] (MyHash::RecordType record) {
            key_buf = record.key ();
            val_buf = record.value ();
        };
        for (int i = 0; i < 100; i++) {
            auto res = mapi.Find (i, thread_info, read_callback);
            if (!res) {
                printf ("Fail get\n");
            }
            INFO ("Get integer key: %d, val: %f\n", i, val_buf);
        }
    }

    {
        typedef turbo_pmem::unordered_map<double, std::string> MyHash;
        MyHash mapi;
        mapi.Initialize (2, 32);
        auto thread_info = mapi.getThreadInfo ();
        INFO ("HashSlot size: %lu\n", sizeof (MyHash::HashSlot));

        for (int i = 0; i < 100; i++) {
            mapi.Put (i * 1.01, "value" + std::to_string (i), thread_info);
        }

        double key_buf;
        std::string val_buf;
        auto read_callback = [&] (MyHash::RecordType record) {
            key_buf = record.key ();
            val_buf = record.value ();
        };
        for (int i = 0; i < 100; i++) {
            auto res = mapi.Find (i * 1.01, thread_info, read_callback);
            if (!res) {
                printf ("Fail get\n");
            }
            INFO ("Get double key: %f, val: %s\n", key_buf, val_buf.c_str ());
        }
    }

    {
        typedef turbo_pmem::unordered_map<std::string, double> MyHash;
        MyHash mapi;
        mapi.Initialize (2, 32);

        auto thread_info = mapi.getThreadInfo ();
        INFO ("HashSlot size: %lu\n", sizeof (MyHash::HashSlot));
        for (int i = 0; i < 100; i++) {
            mapi.Put ("key" + std::to_string (i), i, thread_info);
        }

        std::string key_buf;
        double val_buf;
        auto read_callback = [&] (MyHash::RecordType record) {
            key_buf = record.key ();
            val_buf = record.value ();
        };
        for (int i = 0; i < 100; i++) {
            auto res = mapi.Find ("key" + std::to_string (i), thread_info, read_callback);
            if (!res) {
                printf ("Fail get\n");
            }
            INFO ("Get str key: %s, val: %f\n", key_buf.c_str (), val_buf);
        }
    }

    {
        typedef turbo_pmem::unordered_map<int, int> MyHash;
        MyHash mapi;
        mapi.Initialize (2, 16);
        auto thread_info = mapi.getThreadInfo ();
        INFO ("HashSlot size: %lu\n", sizeof (MyHash::HashSlot));
        for (int i = 0; i < 100; i++) {
            mapi.Put (i, i, thread_info);
        }

        int key_buf;
        int val_buf;
        auto read_callback = [&] (MyHash::RecordType record) {
            key_buf = record.key ();
            val_buf = record.value ();
        };
        for (int i = 0; i < 100; i++) {
            auto res = mapi.Find (i, thread_info, read_callback);
            if (!res) {
                printf ("Fail get\n");
            }
            INFO ("Get int key: %d, int: %d\n", key_buf, val_buf);
        }

        mapi.Delete (20, thread_info);
        if (mapi.Find (20, thread_info, [&] (MyHash::RecordType record) { return; })) {
            printf ("!!! Cannot delete key\n");
        }

        mapi.PrintAllMeta ();

        mapi.Put (20, 202, thread_info);
        mapi.PrintAllMeta ();
    }

    return 0;
}