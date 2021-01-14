#include <immintrin.h>
#include <cstdlib>
#include <unordered_map>

#ifdef CUCKOO
#include <libcuckoo/cuckoohash_map.hh>
#endif

#include "util/env.h"
#include "turbo/turbo_hash.h"
#include "util/robin_hood.h"
#include "util/io_report.h"
#include "util/trace.h"
#include "util/perf_util.h"

#include "absl/container/flat_hash_map.h"

#include "test_util.h"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

using namespace util;


// ./hash_bench_old --thread_write=8 --thread_read=8
DEFINE_uint32(readtime, 0, "if 0, then we read all keys");
DEFINE_uint64(num, 60 * 1000000, "");
DEFINE_bool(print_thread_read, false, "");
DEFINE_int32(thread_read, 1, "");
DEFINE_int32(thread_write, 1, "");
DEFINE_double(loadfactor, 0.7, "default loadfactor for turbohash.");
DEFINE_int32(cell_count, 64, "");
DEFINE_int32(bucket_count, 128 << 10, "bucket count");
DEFINE_int32(value_size, 24, "default value size");
// use numactl --hardware command to check numa node info
// static int kThreadIDs[16] = {16, 17, 18, 19, 20, 21, 22, 23, 0, 1, 2, 3, 4, 5, 6, 7 };

typedef turbo::unordered_map<size_t, std::string> TurboHash;

class HashBench {
public:
    HashBench(int value_size):
        max_count_(FLAGS_num),
        key_trace_(max_count_),
        value_(value_size, 'v') {
        // arm_sig_int();
        signal(SIGALRM, &sigalrm_handler);  // set a signal handler
    }

    void PrintSpeed(std::string name, double loadfactor, size_t size, size_t inserted, size_t duration_ns, bool read) {
        printf("%33s - Load Factor: %.2f. %s   %10lu key, Size: %10lu. Speed: %6.2f Mops/s. Time: %6.2f s\n", 
            name.c_str(), 
            loadfactor,
            read ? "Read  " : "Insert",
            inserted,
            size,
            (double)inserted / duration_ns * 1000.0, 
            duration_ns / 1000000000.0
            );
    }

    size_t TestRehash() {
        size_t inserted_num = 0;
        util::Stats stats;
        TurboHash hashtable(FLAGS_bucket_count, FLAGS_cell_count);
        uint64_t i = 0;
        RandomKeyTrace::Iterator key_iterator = key_trace_.trace_at(0, max_count_);
        
        auto time_start = Env::Default()->NowNanos();
        while (i < max_count_) {
            auto res = hashtable.Put(key_iterator.Next(), value_);
            if ((i++ & 0xFFFFF) == 0) {
                fprintf(stderr, "insert%*s-%03d\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
            }
        }
        auto time_end = Env::Default()->NowNanos();

        std::string name = "turbo:" + hashtable.ProbeStrategyName();
        inserted_num = i;
        PrintSpeed(name, hashtable.LoadFactor(), hashtable.Size(), inserted_num, time_end - time_start, false);
        
        auto read_fun = [&]{
            key_trace_.Randomize();
            std::vector<std::thread> workers(FLAGS_thread_read);
            std::vector<size_t> counts(FLAGS_thread_read, 0);
            kRunning = true;
            if (FLAGS_readtime != 0) alarm(FLAGS_readtime);  // set an alarm for 6 seconds from now
            auto time_start = Env::Default()->NowNanos();
            for (int t = 0; t < FLAGS_thread_read; t++) {
                workers[t] = std::thread([&, t]
                {                 
                    size_t i = 0;
                    TurboHash::value_type* res;
                    size_t start_offset = random() % inserted_num;
                    if (FLAGS_print_thread_read) printf("thread %2d trace offset: %10lu\n", t, start_offset);
                    auto key_iterator = key_trace_.trace_at(start_offset, inserted_num);
                    while (kRunning && key_iterator.Valid()) {
                        res = hashtable.Find(key_iterator.Next());
                        if ((i++ & 0xFFFFF) == 0) {
                            fprintf(stderr, "thread: %2d reading%*s-%03d->%s\r", t,  int(i >> 20), " ", int(i >> 20), res->second().c_str());fflush(stderr);
                        }
                    }
                    if (FLAGS_print_thread_read) printf("thread: %2d, search res: %s. iter info: %s. running: %s\n", t, res != nullptr ? "true" : "false", key_iterator.Info().c_str(), kRunning ? "true" : "false");
                    counts[t] = i ;
                });
            }
            std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
            {
                t.join();
            });
            auto time_end = Env::Default()->NowNanos();
            size_t read_count = std::accumulate(counts.begin(), counts.end(), 0);
            PrintSpeed("Get " + name, hashtable.LoadFactor(), hashtable.Size(), read_count, time_end - time_start, true);
            if (FLAGS_print_thread_read)
                for(int i = 0; i < FLAGS_thread_read; i++) {
                    printf("thread %2d read: %10lu\n", i, counts[i]);
                }
        };

        auto find_fun = [&]{
            key_trace_.Randomize();
            std::vector<std::thread> workers(FLAGS_thread_read);
            std::vector<size_t> counts(FLAGS_thread_read, 0);
            kRunning = true;
            if (FLAGS_readtime != 0) alarm(FLAGS_readtime);  // set an alarm for 6 seconds from now
            auto time_start = Env::Default()->NowNanos();
            for (int t = 0; t < FLAGS_thread_read; t++) {
                workers[t] = std::thread([&, t]
                {
                    std::string value;
                    TurboHash::value_type* record_ptr;
                    size_t i = 0;
                    size_t start_offset = random() % inserted_num;
                    if (FLAGS_print_thread_read) printf("thread %2d trace offset: %10lu\n", t, start_offset);
                    auto key_iterator = key_trace_.trace_at(start_offset, inserted_num);
                    while (kRunning && key_iterator.Valid()) {
                        record_ptr = hashtable.Find(key_iterator.Next());
                        if ((i++ & 0xFFFFF) == 0) {
                            fprintf(stderr, "thread: %2d finding%*s-%03d->%s\r", t,  int(i >> 20), " ", int(i >> 20), record_ptr->second().c_str());fflush(stderr);
                        }
                    }
                    if (FLAGS_print_thread_read) printf("thread: %2d, search res: %s. iter info: %s. running: %s\n", t, record_ptr ? "true" : "false", key_iterator.Info().c_str(), kRunning ? "true" : "false");
                    counts[t] = i ;
                });
            }
            std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
            {
                t.join();
            });
            auto time_end = Env::Default()->NowNanos();
            size_t read_count = std::accumulate(counts.begin(), counts.end(), 0);
            PrintSpeed("Find " + name, hashtable.LoadFactor(), hashtable.Size(), read_count, time_end - time_start, true);
            if (FLAGS_print_thread_read)
                for(int i = 0; i < FLAGS_thread_read; i++) {
                    printf("thread %2d read: %10lu\n", i, counts[i]);
                }
        };

        auto probe_fun = [&]{
            key_trace_.Randomize();
            std::vector<std::thread> workers(FLAGS_thread_read);
            std::vector<size_t> counts(FLAGS_thread_read, 0);
            kRunning = true;
            if (FLAGS_readtime != 0) alarm(FLAGS_readtime);  // set an alarm for 6 seconds from now
            auto time_start = Env::Default()->NowNanos();
            for (int t = 0; t < FLAGS_thread_read; t++) {
                workers[t] = std::thread([&, t]
                {
                    std::string value;
                    TurboHash::value_type* val_ptr;
                    size_t i = 0;
                    size_t start_offset = random() % inserted_num;
                    if (FLAGS_print_thread_read) printf("thread %2d trace offset: %10lu\n", t, start_offset);
                    auto key_iterator = key_trace_.trace_at(start_offset, inserted_num);
                    while (kRunning && key_iterator.Valid()) {
                        val_ptr = hashtable.Probe(key_iterator.Next());
                        if ((i++ & 0xFFFFF) == 0) {
                            fprintf(stderr, "thread: %2d probing%*s-%03d->%s\r", t,  int(i >> 20), " ", int(i >> 20), val_ptr->second().c_str());fflush(stderr);
                        }
                    }
                    if (FLAGS_print_thread_read) printf("thread: %2d, search res: %s. iter info: %s. running: %s\n", t, val_ptr != nullptr ? "true" : "false", key_iterator.Info().c_str(), kRunning ? "true" : "false");
                    counts[t] = i ;
                });
            }
            std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
            {
                t.join();
            });
            auto time_end = Env::Default()->NowNanos();
            size_t read_count = std::accumulate(counts.begin(), counts.end(), 0);
            PrintSpeed("Probe " + name, hashtable.LoadFactor(), hashtable.Size(), read_count, time_end - time_start, true);
            if (FLAGS_print_thread_read)
                for(int i = 0; i < FLAGS_thread_read; i++) {
                    printf("thread %2d read: %10lu\n", i, counts[i]);
                }
        };      


        read_fun();
        find_fun();
        probe_fun();

        // printf("Start Rehashing\n");
        // time_start = Env::Default()->NowMicros();
        // hashtable.MinorReHashAll();
        // time_end   = Env::Default()->NowMicros();
        // printf("rehash speed (%lu entries): %f Mops/s. duration: %.2f s.\n", hashtable.Size(), (double)hashtable.Size() / (time_end - time_start), (double)(time_end - time_start) / 1000000.0 );

        // read_fun();
        // find_fun();
        // probe_fun();

        return inserted_num;
    }


    size_t TurboHashSpeedTest() {
        util::Stats stats;
        TurboHash hashtable(FLAGS_bucket_count, FLAGS_cell_count);
        std::string name = "turbo:" + hashtable.ProbeStrategyName();
        size_t max_range = max_count_ * FLAGS_loadfactor;
        {
            std::vector<std::thread> workers(FLAGS_thread_write);
            std::vector<size_t> counts(FLAGS_thread_write, 0);
            kRunning = true;
            auto time_start = Env::Default()->NowNanos();
            for (int t = 0; t < FLAGS_thread_write; t++) {
                workers[t] = std::thread([&, t] {
                    // Env::PinCore(kThreadIDs[t]);
                    uint64_t i = 0;
                    bool res = true;
                    size_t start_offset = random() % max_range;
                    auto key_iterator = key_trace_.trace_at(start_offset, max_range);
                    while (res && i < max_range) {
                        res = hashtable.Put(key_iterator.Next(), value_);
                        if ((i++ & 0xFFFFF) == 0) {
                            fprintf(stderr, "thread: %2d inserting%*s-%03d->\r", t, int(i >> 20), " ", int(i >> 20));fflush(stderr);
                        }
                    }
                    counts[t] = i - 1;
                });
            }   
            std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
            {
                t.join();
            });
            auto time_end = Env::Default()->NowNanos();
            size_t write_count = std::accumulate(counts.begin(), counts.end(), 0);
            printf("Total put: %lu\n", write_count);
            PrintSpeed(name, hashtable.LoadFactor(), hashtable.Size(), write_count, time_end - time_start, false);
            
        }

        {
            auto read_fun = [&]{
                key_trace_.Randomize();
                std::vector<std::thread> workers(FLAGS_thread_read);
                std::vector<size_t> counts(FLAGS_thread_read, 0);
                kRunning = true;
                if (FLAGS_readtime != 0) alarm(FLAGS_readtime);  // set an alarm for 6 seconds from now
                auto time_start = Env::Default()->NowNanos();
                for (int t = 0; t < FLAGS_thread_read; t++) {
                    workers[t] = std::thread([&, t]
                    {                        
                        size_t i = 0;
                        size_t start_offset = random() % max_range;
                        TurboHash::value_type* res;
                        if (FLAGS_print_thread_read) printf("thread %2d trace offset: %10lu\n", t, start_offset);
                        auto key_iterator = key_trace_.trace_at(start_offset, max_range);
                        while (kRunning && key_iterator.Valid()) {
                            res = hashtable.Find(key_iterator.Next());
                            if ((i++ & 0xFFFFF) == 0) {
                                fprintf(stderr, "thread: %2d reading%*s-%03d->%s\r", t,  int(i >> 20), " ", int(i >> 20), res->second().c_str());fflush(stderr);
                            }
                        }
                        if (FLAGS_print_thread_read) printf("thread: %2d, search res: %s. iter info: %s. running: %s\n", t, res != nullptr ? "true" : "false", key_iterator.Info().c_str(), kRunning ? "true" : "false");
                        counts[t] = i ;
                    });
                }
                std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
                {
                    t.join();
                });
                auto time_end = Env::Default()->NowNanos();
                size_t read_count = std::accumulate(counts.begin(), counts.end(), 0);
                PrintSpeed(name, hashtable.LoadFactor(), hashtable.Size(), read_count, time_end - time_start, true);
                if (FLAGS_print_thread_read)
                for(int i = 0; i < FLAGS_thread_read; i++) {
                    printf("thread %2d read: %10lu\n", i, counts[i]);
                }
            };
            read_fun();
        }

        return max_range;
    }
    
    template <class HashMap, class ValueType>
    void HashSpeedTest(const std::string& name, size_t inserted_num) {
        HashMap map;
        std::string key = "ltkey";
        uint64_t i = 0;
        map.reserve(inserted_num);
        auto key_iterator = key_trace_.trace_at(0, inserted_num);
        auto time_start = Env::Default()->NowNanos();
        while (key_iterator.Valid()) {
            map.insert({key_iterator.Next(), value_});
            if ((i++ & 0xFFFFF) == 0) {
                fprintf(stderr, "inserting%*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
            }
        }
        auto time_end = Env::Default()->NowNanos();
        PrintSpeed(name.c_str(), map.load_factor(), map.size(), inserted_num, time_end - time_start, false);

        {
            std::vector<std::thread> workers(FLAGS_thread_read);
            std::vector<size_t> counts(FLAGS_thread_read, 0);
            kRunning = true;
            key_trace_.Randomize();
            if (FLAGS_readtime != 0) alarm(FLAGS_readtime);  // set an alarm for 6 seconds from now
            time_start = Env::Default()->NowNanos();
            for (int t = 0; t < FLAGS_thread_read; t++) {            
                workers[t] = std::thread([&, t]
                {
                    size_t i = 0;
                    auto iter = map.begin();
                    std::string value;
                    size_t start_offset = random() % inserted_num;
                    if (FLAGS_print_thread_read) printf("thread %2d trace offset: %10lu\n", t, start_offset);
                    auto key_iterator = key_trace_.trace_at(start_offset, inserted_num);
                    while (kRunning && key_iterator.Valid() && iter != map.end()) {
                        iter = map.find(key_iterator.Next());
                        value = iter->second;
                        if ((i++ & 0xFFFFF) == 0) {
                            fprintf(stderr, "thread: %2d reading%*s-%03d->%s\r", t, int(i >> 20), " ", int(i >> 20), value.c_str());fflush(stderr);
                        }
                    }
                    if (FLAGS_print_thread_read) printf("thread: %2d, search res: %s. iter info: %s. running: %s\n", t, iter != map.end() ? "true" : "false", key_iterator.Info().c_str(), kRunning ? "true" : "false");
                    counts[t] = i ;
                });
            }
            std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
            {
                t.join();
            });
            time_end = Env::Default()->NowNanos();
            size_t read_count = std::accumulate(counts.begin(), counts.end(), 0);
            PrintSpeed(name.c_str(), map.load_factor(), map.size(), read_count, time_end - time_start, true);
            if (FLAGS_print_thread_read)
            for(int i = 0; i < FLAGS_thread_read; i++) {
                printf("thread %2d read: %10lu\n", i, counts[i]);
            }
        }
        
        {
            std::vector<std::thread> workers(FLAGS_thread_read);
            std::vector<size_t> counts(FLAGS_thread_read, 0);
            kRunning = true;
            key_trace_.Randomize();
            if (FLAGS_readtime != 0) alarm(FLAGS_readtime);  // set an alarm for 6 seconds from now
            time_start = Env::Default()->NowNanos();
            for (int t = 0; t < FLAGS_thread_read; t++) {            
                workers[t] = std::thread([&, t]
                {
                    size_t i = 0;
                    auto iter = map.begin();
                    size_t start_offset = random() % inserted_num;
                    if (FLAGS_print_thread_read) printf("thread %2d trace offset: %10lu\n", t, start_offset);
                    auto key_iterator = key_trace_.trace_at(start_offset, inserted_num);
                    while (kRunning && key_iterator.Valid() && iter != map.end()) {
                        iter = map.find(key_iterator.Next());
                        if ((i++ & 0xFFFFF) == 0) {
                            fprintf(stderr, "thread: %2d probing%*s-%03d->%s\r", t, int(i >> 20), " ", int(i >> 20), iter->second.c_str());fflush(stderr);
                        }
                    }
                    if (FLAGS_print_thread_read) printf("thread: %2d, search res: %s. iter info: %s. running: %s\n", t, iter != map.end() ? "true" : "false", key_iterator.Info().c_str(), kRunning ? "true" : "false");
                    counts[t] = i ;
                });
            }
            std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
            {
                t.join();
            });
            time_end = Env::Default()->NowNanos();
            size_t read_count = std::accumulate(counts.begin(), counts.end(), 0);
            PrintSpeed("Probe " + name, map.load_factor(), map.size(), read_count, time_end - time_start, true);
            if (FLAGS_print_thread_read)
            for(int i = 0; i < FLAGS_thread_read; i++) {
                printf("thread %2d read: %10lu\n", i, counts[i]);
            }
        }
    } 

    #ifdef CUCKOO
    void CuckooSpeedTest(const std::string& name, size_t inserted_num) {
        libcuckoo::cuckoohash_map<std::string, std::string> map;
        uint64_t i = 0;
        map.reserve(inserted_num);
        auto key_iterator = key_trace_.trace_at(0, inserted_num);
        auto time_start = Env::Default()->NowNanos();
        while (key_iterator.Valid()) {
            map.insert(key_iterator.Next(), value_);
            if ((i++ & 0xFFFFF) == 0) {
                fprintf(stderr, "inserting%*s-%03d->\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
            }
        }
        auto time_end = Env::Default()->NowNanos();
        PrintSpeed(name.c_str(), map.load_factor(), map.size(), i, time_end - time_start, false);

        std::vector<std::thread> workers(FLAGS_thread_read);
        std::vector<size_t> counts(FLAGS_thread_read, 0);
        kRunning = true;
        if (FLAGS_readtime != 0) alarm(FLAGS_readtime);  // set an alarm for 6 seconds from now
        time_start = Env::Default()->NowNanos();
        for (int t = 0; t < FLAGS_thread_read; t++) {
            workers[t] = std::thread([&, t]
            {
                // core function
                // Env::PinCore(kThreadIDs[t]);
                size_t i = 0;
                std::string out;
                bool is_find = false;
                size_t start_offset = random() % inserted_num;
                if (FLAGS_print_thread_read) printf("thread %2d trace offset: %10lu\n", t, start_offset);
                auto key_iterator = key_trace_.trace_at(start_offset, inserted_num);
                do {
                    is_find = map.find(key_iterator.Next(), out);
                    if ((i++ & 0xFFFFF) == 0) {
                        fprintf(stderr, "thread: %2d reading%*s-%03d->\r", t, int(i >> 20), " ", int(i >> 20));fflush(stderr);
                    }
                } while (kRunning && key_iterator.Valid() && is_find);
                if (FLAGS_print_thread_read) printf("thread: %2d, search res: %s. iter info: %s, running: %s\n", t, is_find ? "true" : "false", key_iterator.Info().c_str(), kRunning ? "true" : "false");
                counts[t] = i ;
            });
        }
        std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
        {
            t.join();
        });
        time_end = Env::Default()->NowNanos();
        size_t read_count = std::accumulate(counts.begin(), counts.end(), 0);
        PrintSpeed(name.c_str(), map.load_factor(), map.size(), read_count, time_end - time_start, true);
    
        if (FLAGS_print_thread_read)
        for(int i = 0; i < FLAGS_thread_read; i++) {
            printf("thread %2d read: %10lu\n", i, counts[i]);
        }
    }
    #endif
    

    static void sig_int(int sig)
    {
        printf("Exiting on signal %d\r", sig);
        kRunning = false;
    }

    static void arm_sig_int(void)
    {
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        act.sa_handler = sig_int;
        act.sa_flags = SA_RESTART;
        sigaction(SIGINT, &act, NULL);
    }

    static void sigalrm_handler(int sig)
    {
        printf("Exiting on timeout %d\r", sig);
        kRunning = false;
    }

private:
    size_t max_count_;
    RandomKeyTrace key_trace_;
    std::string value_;
    static bool kRunning;
};
bool HashBench::kRunning = true;

int main(int argc, char *argv[]) {
    debug_perf_ppid();
    ParseCommandLineFlags(&argc, &argv, true);

    HashBench hash_bench(FLAGS_value_size);
    // size_t inserted_num = 0;
    // inserted_num = hash_bench.TurboHashSpeedTest();
    // printf("Inserted: %lu\n", inserted_num);
    // hash_bench.HashSpeedTest<robin_hood::unordered_map<size_t, std::string>, std::string >("robin_hood::unordered_map", FLAGS_num);
    // hash_bench.HashSpeedTest<absl::flat_hash_map<size_t, std::string>, std::string >("absl::flat_hash_map", FLAGS_num);
    // hash_bench.HashSpeedTest<std::unordered_map<size_t, std::string>, std::string >("std::unordered_map", FLAGS_num);
    hash_bench.TestRehash();
    // hash_bench.CuckooSpeedTest("CuckooHashMap", inserted_num);
    return 0;
}

