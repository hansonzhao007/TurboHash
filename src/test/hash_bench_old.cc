#include <immintrin.h>
#include <cstdlib>
#include <unordered_map>

#include "turbo/turbo_hash.h"
#include "util/perf_util.h"

#include "util/slice.h"
#include "util/time.h"

#include "test_util.h"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

using namespace util;


// ./hash_bench_old --thread_write=8 --thread_read=8
DEFINE_uint32(readtime, 0, "if 0, then we read all keys");
DEFINE_uint64(num, 50 * 1000000, "");
DEFINE_bool(print_thread_read, false, "");
DEFINE_int32(thread_read, 1, "");
DEFINE_int32(thread_write, 1, "");
DEFINE_double(loadfactor, 0.7, "default loadfactor for turbohash.");
DEFINE_int32(cell_count, 32, "");
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
        TurboHash hashtable(FLAGS_bucket_count, FLAGS_cell_count);
        uint64_t i = 0;
        RandomKeyTrace::Iterator key_iterator = key_trace_.trace_at(0, max_count_);
        
        auto time_start = util::NowNanos();
        while (i < max_count_) {
            hashtable.Put(key_iterator.Next(), value_);
            if ((i++ & 0xFFFFF) == 0) {
                fprintf(stderr, "insert%*s-%03d\r", int(i >> 20), " ", int(i >> 20));fflush(stderr);
            }
        }
        auto time_end = util::NowNanos();

        std::string name = "turbo:" + hashtable.ProbeStrategyName();
        inserted_num = i;
        PrintSpeed(name, hashtable.LoadFactor(), hashtable.Size(), inserted_num, time_end - time_start, false);
        
        auto read_fun = [&]{
            key_trace_.Randomize();
            std::vector<std::thread> workers(FLAGS_thread_read);
            std::vector<size_t> counts(FLAGS_thread_read, 0);
            kRunning = true;
            if (FLAGS_readtime != 0) alarm(FLAGS_readtime);  // set an alarm for 6 seconds from now
            auto time_start = util::NowNanos();
            for (int t = 0; t < FLAGS_thread_read; t++) {
                workers[t] = std::thread([&, t]
                {                 
                    size_t i = 0;
                    TurboHash::value_type* res = nullptr;
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
            auto time_end = util::NowNanos();
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
            auto time_start = util::NowNanos();
            for (int t = 0; t < FLAGS_thread_read; t++) {
                workers[t] = std::thread([&, t]
                {
                    std::string value;
                    TurboHash::value_type* record_ptr = nullptr;
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
            auto time_end = util::NowNanos();
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
            auto time_start = util::NowNanos();
            for (int t = 0; t < FLAGS_thread_read; t++) {
                workers[t] = std::thread([&, t]
                {
                    std::string value;
                    TurboHash::value_type* val_ptr = nullptr;
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
            auto time_end = util::NowNanos();
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

        printf("Start Rehashing\n");        
        hashtable.MinorReHashAll();

        read_fun();
        find_fun();
        probe_fun();

        return inserted_num;
    }


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
    size_t inserted_num = 0;
    hash_bench.TestRehash();
    return 0;
}

