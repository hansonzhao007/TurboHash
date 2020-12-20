#include <immintrin.h>
#include <cstdlib>

#ifdef CUCKOO
#include <libcuckoo/cuckoohash_map.hh>
#endif

#include "turbo/turbo_hash.h"

#include "util/env.h"
#include "util/robin_hood.h"
#include "util/io_report.h"
#include "util/trace.h"
#include "util/perf_util.h"
#include "util/histogram.h"
#include "util/port_posix.h"

#include "absl/container/flat_hash_map.h"

#include "test_util.h"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

using namespace util;

// For hash table 
DEFINE_uint32(associate_size, 64, "");
DEFINE_uint32(bucket_size, 128 << 10, "bucket count");
DEFINE_double(loadfactor, 0.68, "default loadfactor for turbohash.");
DEFINE_uint32(probe_type, 0, "\
    0: probe within bucket, \
    1: probe within cell");
DEFINE_uint32(cell_type, 0, "\
    0: 128 byte cell, \
    1: 256 byte cell");
    
DEFINE_uint32(readtime, 0, "if 0, then we read all keys");
DEFINE_uint32(thread, 1, "");
DEFINE_uint64(report_interval, 0, "Report interval in seconds");
DEFINE_uint64(stats_interval, 10000000, "Report interval in ops");
DEFINE_uint64(value_size, 1, "The value size");
DEFINE_uint64(num, 100 * 1000000LU, "Number of record to operate");
DEFINE_uint64(read,  100000000, "Number of read operations");
DEFINE_uint64(write, 100000000, "Number of read operations");

DEFINE_bool(hist, false, "");

DEFINE_string(benchmarks, "fillrandom,readrandom", "");

// A very simple random number generator.  Not especially good at
// generating truly random bits, but good enough for our needs in this
// package.
namespace {


// turbo::HashTable* HashTableCreate(int cell_type, int probe_type, int bucket, int associate) {
//     if (0 == cell_type && 0 == probe_type)
//         return new turbo::detail::TurboHashTable<turbo::detail::CellMeta128, turbo::detail::ProbeWithinBucket>(bucket, associate);
//     if (0 == cell_type && 1 == probe_type)
//         return new turbo::detail::TurboHashTable<turbo::detail::CellMeta128, turbo::detail::ProbeWithinCell>(bucket, associate);
//     if (1 == cell_type && 0 == probe_type)
//         return new turbo::detail::TurboHashTable<turbo::detail::CellMeta256, turbo::detail::ProbeWithinBucket>(bucket, associate);
//     if (1 == cell_type && 1 == probe_type)
//         return new turbo::detail::TurboHashTable<turbo::detail::CellMeta256, turbo::detail::ProbeWithinCell>(bucket, associate);
//     else
//         return new turbo::detail::TurboHashTable<turbo::detail::CellMeta128, turbo::detail::ProbeWithinBucket>(bucket, associate);
// }
class Random {
private:
    uint32_t seed_;
public:
    explicit Random(uint32_t s) : seed_(s & 0x7fffffffu) {
        // Avoid bad seeds.
        if (seed_ == 0 || seed_ == 2147483647L) {
        seed_ = 1;
        }
    }
    uint32_t Next() {
        static const uint32_t M = 2147483647L;   // 2^31-1
        static const uint64_t A = 16807;  // bits 14, 8, 7, 5, 2, 1, 0
        // We are computing
        //       seed_ = (seed_ * A) % M,    where M = 2^31-1
        //
        // seed_ must not be zero or M, or else all subsequent computed values
        // will be zero or M respectively.  For all other values, seed_ will end
        // up cycling through every number in [1,M-1]
        uint64_t product = seed_ * A;

        // Compute (product % M) using the fact that ((x << 31) % M) == x.
        seed_ = static_cast<uint32_t>((product >> 31) + (product & M));
        // The first reduction may overflow by 1 bit, so we may need to
        // repeat.  mod == M is not possible; using > allows the faster
        // sign-bit-based test.
        if (seed_ > M) {
        seed_ -= M;
        }
        return seed_;
    }
    // Returns a uniformly distributed value in the range [0..n-1]
    // REQUIRES: n > 0
    uint32_t Uniform(int n) { return Next() % n; }

    // Randomly returns true ~"1/n" of the time, and false otherwise.
    // REQUIRES: n > 0
    bool OneIn(int n) { return (Next() % n) == 0; }

    // Skewed: pick "base" uniformly from range [0,max_log] and then
    // return "base" random bits.  The effect is to pick a number in the
    // range [0,2^max_log-1] with exponential bias towards smaller numbers.
    uint32_t Skewed(int max_log) {
        return Uniform(1 << Uniform(max_log + 1));
    }
};


Slice RandomString(Random* rnd, int len, std::string* dst) {
    dst->resize(len);
    for (int i = 0; i < len; i++) {
        (*dst)[i] = static_cast<char>(' ' + rnd->Uniform(95));   // ' ' .. '~'
    }
    return Slice(*dst);
}

Slice CompressibleString(Random* rnd, double compressed_fraction,
                         size_t len, std::string* dst) {
    int raw = static_cast<int>(len * compressed_fraction);
    if (raw < 1) raw = 1;
    std::string raw_data;
    RandomString(rnd, raw, &raw_data);

    // Duplicate the random data until we have filled "len" bytes
    dst->clear();
    while (dst->size() < len) {
        dst->append(raw_data);
    }
    dst->resize(len);
    return Slice(*dst);
}

// Helper for quickly generating random data.
class RandomGenerator {
 private:
    std::string data_;
    int pos_;

 public:
    RandomGenerator() {
        // We use a limited amount of data over and over again and ensure
        // that it is larger than the compression window (32KB), and also
        // large enough to serve all typical value sizes we want to write.
        Random rnd(301);
        std::string piece;
        while (data_.size() < 1048576) {
        // Add a short fragment that is as compressible as specified
        // by FLAGS_compression_ratio.
        CompressibleString(&rnd, 0.5, 100, &piece);
            data_.append(piece);
        }
        pos_ = 0;
    }

  inline turbo::util::Slice Generate(size_t len) {
    if (pos_ + len > data_.size()) {
      pos_ = 0;
      assert(len < data_.size());
    }
    pos_ += len;
    return turbo::util::Slice(data_.data() + pos_ - len, len);
  }
};

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
                "( %.1f,%.1f ) ops/second in (%.6f,%.6f) seconds\n",
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
    
    inline void FinishedBatchOp(int batch) {
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

    inline void FinishedSingleOp(bool is_hist=false) {
        double now = Env::Default()->NowNanos();
        if (is_hist) {
            double nanos = now - last_op_finish_;
            hist_.Add(nanos);
        }
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
  port::Mutex mu;
  port::CondVar cv GUARDED_BY(mu);
  int total GUARDED_BY(mu);

  // Each thread goes through the following states:
  //    (1) initializing
  //    (2) waiting for others to be initialized
  //    (3) running
  //    (4) done

  int num_initialized GUARDED_BY(mu);
  int num_done GUARDED_BY(mu);
  bool start GUARDED_BY(mu);

  SharedState(int total)
      : cv(&mu), total(total), num_initialized(0), num_done(0), start(false) { }
};

// Per-thread state for concurrent executions of the same benchmark.
struct ThreadState {
    int tid;             // 0..n-1 when running in n threads
    // Random rand;         // Has different seeds for different threads
    Stats stats;
    SharedState* shared;
    Trace* trace;
    ThreadState(int index) : 
        tid(index),
        stats(index) {
        trace = new TraceUniform(1123);
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
    turbo::unordered_map<std::string, std::string>* hashtable_;
    RandomKeyTrace* key_trace_;
    size_t max_count_;
    size_t max_range_;
    Benchmark():
        num_(FLAGS_num),
        value_size_(FLAGS_value_size),
        reads_(FLAGS_read),
        writes_(FLAGS_write),
        key_trace_(nullptr) {

        }

    void Run() {
        max_count_ = FLAGS_bucket_size * FLAGS_associate_size * (FLAGS_cell_type == 0 ? 13 : 27);
        max_range_ = max_count_ * FLAGS_loadfactor;
        key_trace_ = new RandomKeyTrace(max_count_);
        PrintHeader();
        bool fresh_db = true;
        // run benchmark
        bool print_hist = FLAGS_hist;        
        const char* benchmarks = FLAGS_benchmarks.c_str();
        int thread = FLAGS_thread;
        while (benchmarks != nullptr) {
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
            if (name == "fillrandom") {
                fresh_db = true;
                method = &Benchmark::DoWrite;                
            } else if (name == "readrandom") {
                fresh_db = false;
                method = &Benchmark::DoRead;                
            }

            if (fresh_db) {
                hashtable_ = new turbo::unordered_map<std::string, std::string>(FLAGS_bucket_size, FLAGS_associate_size);                
            }
            
            if (method != nullptr) RunBenchmark(thread, name, method, print_hist);
        }
    }


    void DoRead(ThreadState* thread) {
        INFO("DoRead");
        uint64_t batch = 1000000;
        if (key_trace_ == nullptr) {
            ERROR("DoRead lack key_trace_ initialization.");
            return;
        }
        size_t start_offset = random() % max_range_;
        auto key_iterator = key_trace_->trace_at(start_offset, max_range_);
        size_t not_find = 0;
        uint64_t data_offset;
        Duration duration(FLAGS_readtime, num_);
        thread->stats.Start();
        while (!duration.Done(batch)) {
            for (uint64_t j = 0; j < batch; j++) {                
                bool res = hashtable_->Find(key_iterator.Next(), data_offset);
                if (unlikely(!res)) {
                    not_find++;
                }
            }
            thread->stats.FinishedBatchOp(batch);
        }
        char buf[100];
        snprintf(buf, sizeof(buf), "(num: %lu, not find: %lu)", num_, not_find);
        thread->stats.AddMessage(buf);
    }

    void DoWrite(ThreadState* thread) {
        INFO("DoWrite");
        RandomGenerator gen;
        uint64_t batch = 100000;
        if (key_trace_ == nullptr) {
            ERROR("DoWrite lack key_trace_ initialization.");
            return;
        }
        size_t start_offset = random() % max_range_;
        auto key_iterator = key_trace_->trace_at(start_offset, max_range_);
        thread->stats.Start();
        std::string val(value_size_, 'v');
        for (uint64_t i = 0; i < num_; i += batch ) {
            for (uint64_t j = 0; j < batch; j++) {                
                bool res = hashtable_->Put(key_iterator.Next(), val);
                if (!res) {
                    INFO("Hash Table Full!!!\n");
                    printf("Hash Table Full!!!\n");
                    goto write_end;
                    
                }
            }
            thread->stats.FinishedBatchOp(batch);
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
            port::MutexLock l(&shared->mu);
            shared->num_initialized++;
            if (shared->num_initialized >= shared->total) {
                shared->cv.SignalAll();
            }
            while (!shared->start) {
                shared->cv.Wait();
            }
        }

        thread->stats.Start();
        (arg->bm->*(arg->method))(thread);
        thread->stats.Stop();

        {
            port::MutexLock l(&shared->mu);
            shared->num_done++;
            if (shared->num_done >= shared->total) {
                shared->cv.SignalAll();
            }
        }
    }

    void RunBenchmark(int n, Slice name,
                    void (Benchmark::*method)(ThreadState*),
                    bool hist) {
        SharedState shared(n);

        ThreadArg* arg = new ThreadArg[n];
        for (int i = 0; i < n; i++) {
            arg[i].bm = this;
            arg[i].method = method;
            arg[i].shared = &shared;
            arg[i].thread = new ThreadState(i);
            arg[i].thread->shared = &shared;
            util::Env::Default()->StartThread(ThreadBody, &arg[i]);
        }

        shared.mu.Lock();
        while (shared.num_initialized < n) {
            shared.cv.Wait();
        }

        shared.start = true;
        shared.cv.SignalAll();
        while (shared.num_done < n) {
            shared.cv.Wait();
        }
        shared.mu.Unlock();

        for (int i = 1; i < n; i++) {
            arg[0].thread->stats.Merge(arg[i].thread->stats);
        }
        arg[0].thread->stats.Report(name, hist);

        for (int i = 0; i < n; i++) {
            delete arg[i].thread;
        }
        delete[] arg;
    }


    void PrintEnvironment() {
        #if defined(__linux)
        time_t now = time(nullptr);
        fprintf(stderr, "Date:              %s", ctime(&now));  // ctime() adds newline

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
            fprintf(stderr, "CPU:               %d * %s\n", num_cpus, cpu_type.c_str());
            fprintf(stderr, "CPUCache:          %s\n", cache_size.c_str());
        }
        #endif
    }

    void PrintHeader() {
        fprintf(stdout, "------------------------------------------------\n");
        PrintEnvironment();
        fprintf(stdout, "Keys:              %d bytes each\n", (int)key_trace_->KeySize());
        fprintf(stdout, "Values:            %d bytes each\n", (int)FLAGS_value_size);
        fprintf(stdout, "Entries:           %lu\n", (uint64_t)num_);
        fprintf(stdout, "Read:              %lu \n", (uint64_t)FLAGS_read);
        fprintf(stdout, "Write:             %lu \n", (uint64_t)FLAGS_write);
        fprintf(stdout, "Thread:            %lu \n", (uint64_t)FLAGS_thread);
        fprintf(stdout, "Buckets:           %lu \n", (uint64_t)FLAGS_bucket_size);
        fprintf(stdout, "Associate:         %lu \n", (uint64_t)FLAGS_associate_size);
        fprintf(stdout, "Report interval:   %lu s\n", (uint64_t)FLAGS_report_interval);
        fprintf(stdout, "Stats interval:    %lu records\n", (uint64_t)FLAGS_stats_interval);
        fprintf(stdout, "benchmarks:        %s\n", FLAGS_benchmarks.c_str());
        fprintf(stdout, "------------------------------------------------\n");
    }
};

int main(int argc, char *argv[])
{
    ParseCommandLineFlags(&argc, &argv, true);
    Benchmark benchmark;
    benchmark.Run();
    return 0;
}
