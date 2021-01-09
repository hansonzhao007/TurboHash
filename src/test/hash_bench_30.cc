#include <immintrin.h>
#include <cstdlib>
#include <thread>               // std::thread
#include <mutex>                // std::mutex
#include <condition_variable>   // std::condition_variable
#include "typename.h"

#ifdef CUCKOO
#include <libcuckoo/cuckoohash_map.hh>
#endif

/* --------- Different HashTable --------*/
#include "turbo/turbo_hash.h"
#include "turbo/turbo_hash_pmem.h"
#include "util/robin_hood.h"
#include "absl/container/flat_hash_map.h"

#include "util/env.h"
#include "util/robin_hood.h"
#include "util/io_report.h"
#include "util/trace.h"
#include "util/perf_util.h"
#include "util/histogram.h"
#include "util/pmm_util.h"

#include "absl/container/flat_hash_map.h"

#include "test_util.h"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

using namespace util;

#define IS_PMEM 1

// For hash table 
DEFINE_bool(use_existing_db, false, "");
DEFINE_bool(no_rehash, false, "control hash table do not do rehashing during insertion");
DEFINE_uint64(cell_count, 16, "");
DEFINE_uint64(bucket_count, 128 << 10, "bucket count");
DEFINE_double(loadfactor, 0.72, "default loadfactor for turbohash.");
DEFINE_uint32(batch, 1000000, "report batch");
DEFINE_uint32(readtime, 0, "if 0, then we read all keys");
DEFINE_uint32(thread, 1, "");
DEFINE_uint64(report_interval, 0, "Report interval in seconds");
DEFINE_uint64(stats_interval, 10000000, "Report interval in ops");
DEFINE_uint64(num, 100 * 1000000LU, "Number of total record");
DEFINE_uint64(read,  100000000, "Number of read operations");
DEFINE_uint64(write, 100000000, "Number of read operations");

DEFINE_bool(hist, false, "");

DEFINE_string(benchmarks, "load,overwrite,readrandom", "");


#ifdef IS_PMEM
typedef turbo_pmem::unordered_map<std::string, std::string> Hashtable;
static bool kIsPmem = true;
#else
typedef turbo::unordered_map<size_t, size_t> Hashtable;
static bool kIsPmem = false;
#endif


namespace {

class Stats {
public:
    int tid_;
    double start_;
    double finish_;
    double seconds_;
    double next_report_time_;
    double last_op_finish_;
    unsigned last_level_compaction_num_;
    HistogramImpl hist_;

    uint64_t done_;
    uint64_t last_report_done_;
    uint64_t last_report_finish_;
    uint64_t next_report_;
    std::string message_;

    Stats() {Start(); }
    explicit Stats(int id) :
        tid_(id){ Start(); }
    
    void Start() {
        start_ = Env::Default()->NowMicros();
        next_report_time_ = start_ + FLAGS_report_interval * 1000000;
        next_report_ = 100;
        last_op_finish_ = start_;
        last_report_done_ = 0;
        last_report_finish_ = start_;
        last_level_compaction_num_ = 0;
        done_ = 0;
        seconds_ = 0;
        finish_ = start_;
        message_.clear();
        hist_.Clear();
    }

    void Merge(const Stats& other) {
        hist_.Merge(other.hist_);
        done_ += other.done_;
        seconds_ += other.seconds_;
        if (other.start_ < start_) start_ = other.start_;
        if (other.finish_ > finish_) finish_ = other.finish_;

        // Just keep the messages from one thread
        if (message_.empty()) message_ = other.message_;
    }
    
    void Stop() {
        finish_ = Env::Default()->NowMicros();
        seconds_ = (finish_ - start_) * 1e-6;;
    }

    void StartSingleOp() {
        last_op_finish_ = Env::Default()->NowMicros();
    }

    void PrintSpeed() {
        uint64_t now = Env::Default()->NowMicros();
        int64_t usecs_since_last = now - last_report_finish_;

        std::string cur_time = TimeToString(now/1000000);
        printf( "%s ... thread %d: (%lu,%lu) ops and "
                "( %.1f,%.1f ) ops/second in (%.4f,%.4f) seconds\n",
                cur_time.c_str(), 
                tid_,
                done_ - last_report_done_, done_,
                (done_ - last_report_done_) /
                (usecs_since_last / 1000000.0),
                done_ / ((now - start_) / 1000000.0),
                (now - last_report_finish_) / 1000000.0,
                (now - start_) / 1000000.0);
        INFO( "%s ... thread %d: (%lu,%lu) ops and "
                "( %.1f,%.1f ) ops/second in (%.6f,%.6f) seconds\n",
                cur_time.c_str(), 
                tid_,
                done_ - last_report_done_, done_,
                (done_ - last_report_done_) /
                (usecs_since_last / 1000000.0),
                done_ / ((now - start_) / 1000000.0),
                (now - last_report_finish_) / 1000000.0,
                (now - start_) / 1000000.0);
        last_report_finish_ = now;
        last_report_done_ = done_;
        fflush(stdout);
    }
    
    static void AppendWithSpace(std::string* str, const std::string& msg) {
        if (msg.empty()) return;
        if (!str->empty()) {
            str->push_back(' ');
        }
        str->append(msg.data(), msg.size());
    }

    void AddMessage(const std::string& msg) {
        AppendWithSpace(&message_, msg);
    }
    
    inline void FinishedBatchOp(size_t batch) {
        double now = Env::Default()->NowNanos();
        last_op_finish_ = now;
        done_ += batch;
        if (unlikely(done_ >= next_report_)) {
            if      (next_report_ < 1000)   next_report_ += 100;
            else if (next_report_ < 5000)   next_report_ += 500;
            else if (next_report_ < 10000)  next_report_ += 1000;
            else if (next_report_ < 50000)  next_report_ += 5000;
            else if (next_report_ < 100000) next_report_ += 10000;
            else if (next_report_ < 500000) next_report_ += 50000;
            else                            next_report_ += 100000;
            fprintf(stderr, "... finished %llu ops%30s\r", (unsigned long long )done_, "");
            
            if (FLAGS_report_interval == 0 && (done_ % FLAGS_stats_interval) == 0) {
                PrintSpeed(); 
                return;
            } 
            fflush(stderr);
            fflush(stdout);
        }

        if (FLAGS_report_interval != 0 && Env::Default()->NowMicros() > next_report_time_) {
            next_report_time_ += FLAGS_report_interval * 1000000;
            PrintSpeed(); 
        }
    }

    inline void FinishedSingleOp() {
        double now = Env::Default()->NowNanos();
        last_op_finish_ = now;

        done_++;
        if (done_ >= next_report_) {
            if      (next_report_ < 1000)   next_report_ += 100;
            else if (next_report_ < 5000)   next_report_ += 500;
            else if (next_report_ < 10000)  next_report_ += 1000;
            else if (next_report_ < 50000)  next_report_ += 5000;
            else if (next_report_ < 100000) next_report_ += 10000;
            else if (next_report_ < 500000) next_report_ += 50000;
            else                            next_report_ += 100000;
            fprintf(stderr, "... finished %llu ops%30s\r", (unsigned long long )done_, "");
            
            if (FLAGS_report_interval == 0 && (done_ % FLAGS_stats_interval) == 0) {
                PrintSpeed(); 
                return;
            } 
            fflush(stderr);
            fflush(stdout);
        }

        if (FLAGS_report_interval != 0 && Env::Default()->NowMicros() > next_report_time_) {
            next_report_time_ += FLAGS_report_interval * 1000000;
            PrintSpeed(); 
        }
    }

    std::string TimeToString(uint64_t secondsSince1970) {
        const time_t seconds = (time_t)secondsSince1970;
        struct tm t;
        int maxsize = 64;
        std::string dummy;
        dummy.reserve(maxsize);
        dummy.resize(maxsize);
        char* p = &dummy[0];
        localtime_r(&seconds, &t);
        snprintf(p, maxsize,
                "%04d/%02d/%02d-%02d:%02d:%02d ",
                t.tm_year + 1900,
                t.tm_mon + 1,
                t.tm_mday,
                t.tm_hour,
                t.tm_min,
                t.tm_sec);
        return dummy;
    }

    void Report(const Slice& name, bool print_hist = false) {
        // Pretend at least one op was done in case we are running a benchmark
        // that does not call FinishedSingleOp().
        if (done_ < 1) done_ = 1;

        std::string extra;

        AppendWithSpace(&extra, message_);

        double elapsed = (finish_ - start_) * 1e-6;

        double throughput = (double)done_/elapsed;
        
        printf( "%-12s : %11.3f micros/op %lf Mops/s;%s%s\n",
                name.ToString().c_str(),
                elapsed * 1e6 / done_,
                throughput/1024/1024,
                (extra.empty() ? "" : " "),
                extra.c_str());
        INFO(   "%-12s : %11.3f micros/op %lf Mops/s;%s%s\n",
                name.ToString().c_str(),
                elapsed * 1e6 / done_,
                throughput/1024/1024,
                (extra.empty() ? "" : " "),
                extra.c_str());
        if (print_hist) {
            fprintf(stdout, "Nanoseconds per op:\n%s\n", hist_.ToString().c_str());
        }

        fflush(stdout);
        fflush(stderr);
    }
};


// State shared by all concurrent executions of the same benchmark.
struct SharedState {
  std::mutex mu;
  std::condition_variable cv;
  int total;

  // Each thread goes through the following states:
  //    (1) initializing
  //    (2) waiting for others to be initialized
  //    (3) running
  //    (4) done

  int num_initialized;
  int num_done;
  bool start;

  SharedState(int total):
    total(total), num_initialized(0), num_done(0), start(false) { }
};

// Per-thread state for concurrent executions of the same benchmark.
struct ThreadState {
    int tid;             // 0..n-1 when running in n threads
    // Random rand;         // Has different seeds for different threads
    Stats stats;
    SharedState* shared;
    ThreadState(int index) : 
        tid(index),
        stats(index) {
    }
};


class Duration {
public:
    Duration(uint64_t max_seconds, int64_t max_ops, int64_t ops_per_stage = 0) {
        max_seconds_ = max_seconds;
        max_ops_= max_ops;
        ops_per_stage_ = (ops_per_stage > 0) ? ops_per_stage : max_ops;
        ops_ = 0;
        start_at_ = Env::Default()->NowMicros();
    }

    inline int64_t GetStage() { return std::min(ops_, max_ops_ - 1) / ops_per_stage_; }

    inline bool Done(int64_t increment) {
        if (increment <= 0) increment = 1;    // avoid Done(0) and infinite loops
        ops_ += increment;

        if (max_seconds_) {
        // Recheck every appx 1000 ops (exact iff increment is factor of 1000)
        auto granularity = 1000;
        if ((ops_ / granularity) != ((ops_ - increment) / granularity)) {
            uint64_t now = Env::Default()->NowMicros();
            return ((now - start_at_) / 1000000) >= max_seconds_;
        } else {
            return false;
        }
        } else {
        return ops_ > max_ops_;
        }
    }

    inline int64_t Ops() {
        return ops_;
    }
 private:
    uint64_t max_seconds_;
    int64_t max_ops_;
    int64_t ops_per_stage_;
    int64_t ops_;
    uint64_t start_at_;
};


#if defined(__linux)
static std::string TrimSpace(std::string s) {
    size_t start = 0;
    while (start < s.size() && isspace(s[start])) {
        start++;
    }
    size_t limit = s.size();
    while (limit > start && isspace(s[limit-1])) {
        limit--;
    }
    return std::string(s.data() + start, limit - start);
}
#endif

}
class Benchmark {

public:
    uint64_t num_;
    int value_size_;
    size_t reads_;
    size_t writes_;
    Hashtable* hashtable_ = nullptr;
    RandomKeyTraceString* key_trace_;
    size_t trace_size_;
    size_t initial_capacity_;
    Benchmark():
        num_(FLAGS_num),
        value_size_(KEY_LEN),
        reads_(FLAGS_read),
        writes_(FLAGS_write),
        key_trace_(nullptr) {

        }
    ~Benchmark() {
        if (hashtable_ != nullptr) {
            delete hashtable_;
        }
    }
    void Run() {
        initial_capacity_ = FLAGS_bucket_count * FLAGS_cell_count * (Hashtable::CellMeta::SlotCount() - 1) ;
        size_t rehash_threshold = initial_capacity_ * FLAGS_loadfactor;

        // If do not rehash, we control the distinct key to the minimum between the FLAGS_num and the rehash threshold.
        if (FLAGS_no_rehash) {
            trace_size_ = std::min(FLAGS_num, rehash_threshold);
            num_ = trace_size_;
        } else {
            trace_size_ = FLAGS_num;
        }
        printf("key trace size: %lu\n", trace_size_);
        key_trace_ = new RandomKeyTraceString(trace_size_);
        if (reads_ == 0) {
            reads_ = key_trace_->count_;
            FLAGS_read = key_trace_->count_;
        }
        PrintHeader();
        bool fresh_db = true;
        // run benchmark
        bool print_hist = false;
        const char* benchmarks = FLAGS_benchmarks.c_str();        
        while (benchmarks != nullptr) {
            int thread = FLAGS_thread;
            void (Benchmark::*method)(ThreadState*) = nullptr;
            const char* sep = strchr(benchmarks, ',');
            std::string name;
            if (sep == nullptr) {
                name = benchmarks;
                benchmarks = nullptr;
            } else {
                name = std::string(benchmarks, sep - benchmarks);
                benchmarks = sep + 1;
            }
            if (name == "load") {
                fresh_db = true;
                method = &Benchmark::DoWrite;                
            } if (name == "loadlat") {
                fresh_db = true;
                print_hist = true;
                method = &Benchmark::DoWriteLat;                
            } else if (name == "overwrite") {
                fresh_db = false;
                key_trace_->Randomize();
                method = &Benchmark::DoOverWrite;                
            } else if (name == "allloadfactor") {
                fresh_db = true;
                method = &Benchmark::DoLoadFactor;                
            } else if (name == "readrandom") {
                fresh_db = false;
                key_trace_->Randomize();
                method = &Benchmark::DoRead;                
            } else if (name == "readnon") {
                fresh_db = false;
                key_trace_->Randomize();
                method = &Benchmark::DoReadNon;                
            } else if (name == "readlat") {
                fresh_db = false;
                print_hist = true;
                key_trace_->Randomize();
                method = &Benchmark::DoReadLat;                
            } else if (name == "readnonlat") {
                fresh_db = false;
                print_hist = true;
                key_trace_->Randomize();
                method = &Benchmark::DoReadNonLat;                
            } else if (name == "rehash") {
                fresh_db = false;
                thread = 1;
                method = &Benchmark::DoRehash;
            } else if (name == "stats") {
                fresh_db = false;
                thread = 1;
                method = &Benchmark::DoStats;
            }

            #ifdef IS_PMEM
            if (fresh_db) {
                if (FLAGS_use_existing_db) {
                    fprintf(stdout, "%-12s : skipped (--use_existing_db is true)\n", name.c_str());
                    method = nullptr;
                } else {
                    remove("/mnt/pmem/turbo_hash_pmem_basemd");
                    remove("/mnt/pmem/turbo_hash_pmem_desc");
                    remove("/mnt/pmem/turbo_hash_pmem_sb");
                    hashtable_ = new Hashtable();
                    hashtable_->Initialize(FLAGS_bucket_count, FLAGS_cell_count);
                }
            } else {
                if (hashtable_ == nullptr && FLAGS_use_existing_db) {
                    hashtable_ = new Hashtable();
                    hashtable_->Recover();
                }
            }
            #else
            if (fresh_db) {
                hashtable_ = new Hashtable(FLAGS_bucket_count, FLAGS_cell_count);
            } else if (hashtable_ == nullptr) {
                perror("Hash table not initialized.");
                exit(1);
            }
            #endif

            #ifdef IS_PMEM
            IPMWatcher watcher(name);
            #endif
            if (method != nullptr) RunBenchmark(thread, name, method, print_hist);
        }
    }

    void DoRehash(ThreadState* thread) {   
        INFO("DoRehash. Thread %2d", thread->tid);  
        thread->stats.Start(); 
        size_t rehash_count = hashtable_->MinorReHashAll();
        thread->stats.FinishedBatchOp(rehash_count);
    }

    void DoStats(ThreadState* thread) {   
        INFO("DoRehash. Thread %2d", thread->tid);  
        thread->stats.Start(); 
        double load_factor = hashtable_->LoadFactor();
        char buf[100];
        snprintf(buf, sizeof(buf), "load factor: %f", load_factor);
        thread->stats.AddMessage(buf);
    }

    void DoRead(ThreadState* thread) {
        INFO("DoRead");
        uint64_t batch = FLAGS_batch;
        if (key_trace_ == nullptr) {
            ERROR("DoRead lack key_trace_ initialization.");
            return;
        }
        size_t start_offset = random() % trace_size_;
        auto key_iterator = key_trace_->trace_at(start_offset, trace_size_);
        size_t not_find = 0;
        uint64_t data_offset;
        Duration duration(FLAGS_readtime, reads_);
        thread->stats.Start();        
        while (!duration.Done(batch) && key_iterator.Valid()) {
            uint64_t j = 0;
            for (; j < batch && key_iterator.Valid(); j++) {                         
                auto record_ptr = hashtable_->Find(key_iterator.Next());
                if (unlikely(record_ptr == nullptr)) {
                    not_find++;
                }
            }
            thread->stats.FinishedBatchOp(j);
        }
        char buf[100];
        snprintf(buf, sizeof(buf), "(num: %lu, not find: %lu)", reads_, not_find);
        INFO("DoRead thread: %2d. Total read num: %lu, not find: %lu)", thread->tid, reads_, not_find);
        thread->stats.AddMessage(buf);
    }

    void DoReadNon(ThreadState* thread) {
        INFO("DoReadNon");
        uint64_t batch = FLAGS_batch;
        if (key_trace_ == nullptr) {
            ERROR("DoReadNon lack key_trace_ initialization.");
            return;
        }
        size_t start_offset = random() % trace_size_;
        auto key_iterator = key_trace_->trace_at(start_offset, trace_size_);
        size_t not_find = 0;
        uint64_t data_offset;
        Duration duration(FLAGS_readtime, reads_);
        thread->stats.Start();        
        while (!duration.Done(batch) && key_iterator.Valid()) {
            uint64_t j = 0;
            for (; j < batch && key_iterator.Valid(); j++) {      
                std::string key = key_iterator.Next();
                key[0] = 'a';                 
                auto record_ptr = hashtable_->Find(key);
                if (likely(record_ptr == nullptr)) {
                    not_find++;
                }
            }
            thread->stats.FinishedBatchOp(j);
        }
        char buf[100];
        snprintf(buf, sizeof(buf), "(num: %lu, not find: %lu)", reads_, not_find);
        INFO("DoReadNon thread: %2d. Total read num: %lu, not find: %lu)", thread->tid, reads_, not_find);
        thread->stats.AddMessage(buf);
    }

    void DoReadLat(ThreadState* thread) {
        INFO("DoReadLat");
        if (key_trace_ == nullptr) {
            ERROR("DoReadLat lack key_trace_ initialization.");
            return;
        }
        size_t start_offset = random() % trace_size_;
        auto key_iterator = key_trace_->trace_at(start_offset, trace_size_);
        size_t not_find = 0;
        uint64_t data_offset;
        Duration duration(FLAGS_readtime, reads_);
        thread->stats.Start();
        while (!duration.Done(1) && key_iterator.Valid()) {       
            auto time_start = Env::Default()->NowNanos();
            auto record_ptr = hashtable_->Find(key_iterator.Next());
            auto time_duration = Env::Default()->NowNanos() - time_start;
            thread->stats.hist_.Add(time_duration);

            if (unlikely(record_ptr == nullptr)) {
                not_find++;
            }       
        }
        char buf[100];
        snprintf(buf, sizeof(buf), "(num: %lu, not find: %lu)", reads_, not_find);
        INFO("DoReadLat thread: %2d. Total read num: %lu, not find: %lu)", thread->tid, reads_, not_find);
        thread->stats.AddMessage(buf);
    }

    void DoReadNonLat(ThreadState* thread) {
        INFO("DoReadNonLat");
        if (key_trace_ == nullptr) {
            ERROR("DoReadNonLat lack key_trace_ initialization.");
            return;
        }
        size_t start_offset = random() % trace_size_;
        auto key_iterator = key_trace_->trace_at(start_offset, trace_size_);
        size_t not_find = 0;
        uint64_t data_offset;
        Duration duration(FLAGS_readtime, reads_);
        thread->stats.Start();
        while (!duration.Done(1) && key_iterator.Valid()) {
            std::string key = key_iterator.Next();
            key[0] = 'a';
            auto time_start = Env::Default()->NowNanos();
            auto record_ptr = hashtable_->Find(key);
            auto time_duration = Env::Default()->NowNanos() - time_start;
            thread->stats.hist_.Add(time_duration);
            if (likely(record_ptr == nullptr)) {
                not_find++;
            } else {
                printf("find key : %lu, val : %lu", record_ptr->first(), record_ptr->second());
                exit(1);
            }
        }
        char buf[100];
        snprintf(buf, sizeof(buf), "(num: %lu, not find: %lu)", reads_, not_find);
        INFO("DoReadNonLat thread: %2d. Total read num: %lu, not find: %lu)", thread->tid, reads_, not_find);
        thread->stats.AddMessage(buf);
    }

    void DoWrite(ThreadState* thread) {
        INFO("DoWrite");
        uint64_t batch = FLAGS_batch;
        if (key_trace_ == nullptr) {
            ERROR("DoWrite lack key_trace_ initialization.");
            return;
        }
        size_t interval = num_ / FLAGS_thread;
        size_t start_offset = thread->tid * interval;
        auto key_iterator = key_trace_->iterate_between(start_offset, start_offset + interval);
        printf("thread %2d, between %lu - %lu\n", thread->tid, start_offset, start_offset + interval);
        thread->stats.Start();
        while (key_iterator.Valid()) {
            uint64_t j = 0;
            for (; j < batch && key_iterator.Valid(); j++) {   
                std::string& key = key_iterator.Next(); 
                bool res = hashtable_->Put(key, key);
                if (!res) {
                    INFO("Hash Table Full!!!\n");
                    printf("Hash Table Full!!!\n");
                    goto write_end;
                }
            }
            thread->stats.FinishedBatchOp(j);
        }
        write_end:
        return;
    }

    void DoWriteLat(ThreadState* thread) {
        INFO("DoWriteLat");
        uint64_t batch = FLAGS_batch;
        if (key_trace_ == nullptr) {
            ERROR("DoWriteLat lack key_trace_ initialization.");
            return;
        }
        size_t interval = num_ / FLAGS_thread;
        size_t start_offset = thread->tid * interval;
        auto key_iterator = key_trace_->iterate_between(start_offset, start_offset + interval);
        printf("thread %2d, between %lu - %lu\n", thread->tid, start_offset, start_offset + interval);
        thread->stats.Start();
        while (key_iterator.Valid()) {                      
            std::string& key = key_iterator.Next();
            auto time_start = Env::Default()->NowNanos();
            bool res = hashtable_->Put(key, key);
            auto time_duration = Env::Default()->NowNanos() - time_start;
            thread->stats.hist_.Add(time_duration);
            
            if (!res) {
                INFO("Hash Table Full!!!\n");
                printf("Hash Table Full!!!\n");
                goto write_end;
            }

        }
        write_end:
        return;
    }

    // Print out load factor every 1 million insertion
    void DoLoadFactor(ThreadState* thread) {
        INFO("DoLoadFactor");
        uint64_t batch = FLAGS_batch;
        if (key_trace_ == nullptr) {
            ERROR("DoLoadFactor lack key_trace_ initialization.");
            return;
        }
        size_t interval = num_ / FLAGS_thread;
        size_t start_offset = thread->tid * interval;
        auto key_iterator = key_trace_->iterate_between(start_offset, start_offset + interval);
        printf("thread %2d, between %lu - %lu\n", thread->tid, start_offset, start_offset + interval);
        size_t inserted = 0;
        thread->stats.Start();        
        while (key_iterator.Valid()) {
            uint64_t j = 0;
            for (; j < batch && key_iterator.Valid(); j++) {   
                std::string& key = key_iterator.Next();                  
                bool res = hashtable_->Put(key, key);
                if (!res) {
                    INFO("Hash Table Full!!!\n");
                    printf("Hash Table Full!!!\n");
                    goto write_end;
                }
                inserted++;
            }
            thread->stats.FinishedBatchOp(j);
            printf("Load factor: %.3f\n", (double)inserted / hashtable_->Capacity());
        }
        write_end:
        return;
    }

    void DoOverWrite(ThreadState* thread) {
        INFO("DoOverWrite");
        uint64_t batch = FLAGS_batch;
        if (key_trace_ == nullptr) {
            ERROR("DoOverWrite lack key_trace_ initialization.");
            return;
        }
        size_t interval = num_ / FLAGS_thread;
        size_t start_offset = thread->tid * interval;
        auto key_iterator = key_trace_->iterate_between(start_offset, start_offset + interval);
        printf("thread %2d, between %lu - %lu\n", thread->tid, start_offset, start_offset + interval);
        thread->stats.Start();
        while (key_iterator.Valid()) {
            uint64_t j = 0;
            for (; j < batch && key_iterator.Valid(); j++) {   
                std::string& key = key_iterator.Next();                  
                bool res = hashtable_->Put(key, key);
                if (!res) {
                    INFO("Hash Table Full!!!\n");
                    printf("Hash Table Full!!!\n");
                    goto write_end;
                    
                }
            }
            thread->stats.FinishedBatchOp(j);
        }
        write_end:
        return;
    }

private:
    struct ThreadArg {
        Benchmark* bm;
        SharedState* shared;
        ThreadState* thread;
        void (Benchmark::*method)(ThreadState*);
    };

    static void ThreadBody(void* v) {
        ThreadArg* arg = reinterpret_cast<ThreadArg*>(v);
        SharedState* shared = arg->shared;
        ThreadState* thread = arg->thread;
        {
            std::unique_lock<std::mutex> lck(shared->mu);
            shared->num_initialized++;
            if (shared->num_initialized >= shared->total) {
                shared->cv.notify_all();
            }
            while (!shared->start) {
                shared->cv.wait(lck);
            }
        }

        thread->stats.Start();
        (arg->bm->*(arg->method))(thread);
        thread->stats.Stop();

        {
            std::unique_lock<std::mutex> lck(shared->mu);
            shared->num_done++;
            if (shared->num_done >= shared->total) {
                shared->cv.notify_all();
            }
        }
    }

    void RunBenchmark(int thread_num, const std::string& name, 
                      void (Benchmark::*method)(ThreadState*), bool print_hist) {
        SharedState shared(thread_num);
        ThreadArg* arg = new ThreadArg[thread_num];
        std::thread server_threads[thread_num];
        for (int i = 0; i < thread_num; i++) {
            arg[i].bm = this;
            arg[i].method = method;
            arg[i].shared = &shared;
            arg[i].thread = new ThreadState(i);
            arg[i].thread->shared = &shared;
            server_threads[i] = std::thread(ThreadBody, &arg[i]);
        }

        std::unique_lock<std::mutex> lck(shared.mu);
        while (shared.num_initialized < thread_num) {
            shared.cv.wait(lck);
        }

        shared.start = true;
        shared.cv.notify_all();
        while (shared.num_done < thread_num) {
            shared.cv.wait(lck);
        }

        for (int i = 1; i < thread_num; i++) {
            arg[0].thread->stats.Merge(arg[i].thread->stats);
        }
        arg[0].thread->stats.Report(name, print_hist);
        
        for (auto& th : server_threads) th.join();
    }


    void PrintEnvironment() {
        #if defined(__linux)
        time_t now = time(nullptr);
        fprintf(stderr, "Date:                  %s", ctime(&now));  // ctime() adds newline

        FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
        if (cpuinfo != nullptr) {
            char line[1000];
            int num_cpus = 0;
            std::string cpu_type;
            std::string cache_size;
            while (fgets(line, sizeof(line), cpuinfo) != nullptr) {
                const char* sep = strchr(line, ':');
                if (sep == nullptr) {
                continue;
                }
                std::string key = TrimSpace(std::string(line, sep - 1 - line));
                std::string val = TrimSpace(std::string(sep + 1));
                if (key == "model name") {
                ++num_cpus;
                cpu_type = val;
                } else if (key == "cache size") {
                cache_size = val;
                }
            }
        fclose(cpuinfo);
        fprintf(stderr, "CPU:                   %d * %s\n", num_cpus, cpu_type.c_str());
        fprintf(stderr, "CPUCache:              %s\n", cache_size.c_str());                  
        }
        #endif
    }

    void PrintHeader() {
                   INFO("------------------------------------------------\n");
        fprintf(stdout, "------------------------------------------------\n");                   
        PrintEnvironment();
        fprintf(stdout, "Pmem:                  %s\n", kIsPmem ? "true" : "false");
                   INFO("Pmem:                  %s\n", kIsPmem ? "true" : "false");
        fprintf(stdout, "Key type:              %s\n", type_name<Hashtable::key_type>().c_str());
                   INFO("Key type:              %s\n", type_name<Hashtable::key_type>().c_str());
        fprintf(stdout, "Val type:              %s\n", type_name<Hashtable::mapped_type>().c_str());
                   INFO("Val type:              %s\n", type_name<Hashtable::mapped_type>().c_str());
        fprintf(stdout, "Keys:                  %lu bytes each\n", KEY_LEN);
                   INFO("Keys:                  %lu bytes each\n", KEY_LEN);
        fprintf(stdout, "Values:                %lu bytes each\n", KEY_LEN);
                   INFO("Values:                %lu bytes each\n", KEY_LEN);
        fprintf(stdout, "Entries:               %lu\n", (uint64_t)num_);
                   INFO("Entries:               %lu\n", (uint64_t)num_);
        fprintf(stdout, "Trace size:            %lu\n", (uint64_t)trace_size_);   
                   INFO("Trace size:            %lu\n", (uint64_t)trace_size_);        
        fprintf(stdout, "Read:                  %lu \n", (uint64_t)FLAGS_read);
                   INFO("Read:                  %lu \n", (uint64_t)FLAGS_read);
        fprintf(stdout, "Write:                 %lu \n", (uint64_t)FLAGS_write);
                   INFO("Write:                 %lu \n", (uint64_t)FLAGS_write);
        fprintf(stdout, "Thread:                %lu \n", (uint64_t)FLAGS_thread);
                   INFO("Thread:                %lu \n", (uint64_t)FLAGS_thread);           
        fprintf(stdout, "Hash key flat:         %s \n", Hashtable::is_key_flat ? "true" : "false");         
                   INFO("Hash key flat:         %s \n", Hashtable::is_key_flat ? "true" : "false");         
        fprintf(stdout, "Hash val flat:         %s \n", Hashtable::is_value_flat ? "true" : "false");         
                   INFO("Hash val flat:         %s \n", Hashtable::is_value_flat ? "true" : "false");         
        fprintf(stdout, "Hash Buckets:          %lu \n", (uint64_t)FLAGS_bucket_count);         
                   INFO("Hash Buckets:          %lu \n", (uint64_t)FLAGS_bucket_count);
        fprintf(stdout, "Hash Cell in Bucket:   %lu \n", (uint64_t)FLAGS_cell_count);
                   INFO("Hash Cell in Bucket:   %lu \n", (uint64_t)FLAGS_cell_count);
        fprintf(stdout, "Hash Slot in Cell:     %u \n", Hashtable::CellMeta::SlotCount());
                   INFO("Hash Slot in Cell:     %u \n", Hashtable::CellMeta::SlotCount());
        fprintf(stdout, "Hash init capacity:    %lu \n", (uint64_t)initial_capacity_);
                   INFO("Hash init capacity:    %lu \n", (uint64_t)initial_capacity_);
        fprintf(stdout, "Hash table size:       %lu MB\n", FLAGS_bucket_count * FLAGS_cell_count * Hashtable::CellMeta::CellSize() / 1024 / 1024);
                   INFO("Hash table size:       %lu MB\n", FLAGS_bucket_count * FLAGS_cell_count * Hashtable::CellMeta::CellSize() / 1024 / 1024);
        fprintf(stdout, "Hash loadfactor:       %.2f \n", FLAGS_loadfactor);
                   INFO("Hash loadfactor:       %.2f \n", FLAGS_loadfactor);
        fprintf(stdout, "Cell Type:             %s \n", Hashtable::CellMeta::Name().c_str()); 
                   INFO("Cell Type:             %s \n", Hashtable::CellMeta::Name().c_str()); 
        fprintf(stdout, "Report interval:       %lu s\n", (uint64_t)FLAGS_report_interval);
                   INFO("Report interval:       %lu s\n", (uint64_t)FLAGS_report_interval);
        fprintf(stdout, "Stats interval:        %lu records\n", (uint64_t)FLAGS_stats_interval);
                   INFO("Stats interval:        %lu records\n", (uint64_t)FLAGS_stats_interval);
        fprintf(stdout, "benchmarks:            %s\n", FLAGS_benchmarks.c_str());
                   INFO("benchmarks:            %s\n", FLAGS_benchmarks.c_str());
        fprintf(stdout, "------------------------------------------------\n");                                      
                   INFO("------------------------------------------------\n");
    }
};

int main(int argc, char *argv[])
{
    ParseCommandLineFlags(&argc, &argv, true);
    Benchmark benchmark;
    benchmark.Run();
    return 0;
}
