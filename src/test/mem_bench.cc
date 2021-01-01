// MIT License

// Copyright (c) [2020] [xingshengzhao@gmail.com]

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <thread>
#include <algorithm>
#include <libpmem.h>
#include <libvmmalloc.h>
#include <random>
#include <iostream>
#include <memory>
#include <numeric>
#include <linux/membarrier.h>
#include <functional>

#include "cpucounters.h"

#include "util/env.h"
#include "util/test_util.h"
#include "util/trace.h"
#include "util/pmm_util.h"


#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

// ******** 6 type ********
// fillrandomNT
// fillrandomWB
// fillseqNT
// fillseqWB
// readrandomNT
// readseqNT
DEFINE_string(type, "fillrandomNT", "");

// ********  mode ********
// single: execute one type of bench using FLAGS_thread
// matrix: iterate every thread and every buffer size, generate a result matrix
// row_block: iterate all buffer size giving thread num, generate a result row
// row_thread: ierate all thread giving buffer size, generate a result row
// wa: test write amplification
// profileWPQ: profiler write pending queue size
// rw: test throughput mixing read and write
DEFINE_string(mode, "row_thread", "");

DEFINE_bool(pmdk, true, "use pmdk lib or not");
DEFINE_string(path, "/mnt/pmem/test.data", "default file path");
DEFINE_int64(filesize, 2ULL << 30, "default file size");
DEFINE_int32(block_size, 256, "unit size");
DEFINE_int32(offset_interval, -1, "offset set interval unit, if not set, will equal to block_size");
DEFINE_int32(thread, 8, "thread num");
DEFINE_uint32(num, 100000000, "number of operations");

// XPBuffer profiling
DEFINE_int32(n_end, 1024, "maximum interval size, uint: 256 byte");
DEFINE_int32(n_start, 1,  "start   interval size, uint: 256 byte");
DEFINE_int32(repeat, 1000000, "repeat overwrite current interval");
DEFINE_int32(buffer, 12, "how many kilo byte of the probe buffer");

DEFINE_bool(load, true, "use avx512 load instruction or not for read");
DEFINE_bool(initfile, false, "initial file or not");

// Maximum length of write buffer size
std::vector<uint64_t> kBufferVector = 
    {64, 128, 256, 512, 1 << 10, 2 << 10, 4 << 10, 8 << 10, 16 << 10, 32 << 10, 64 << 10, 128 << 10, 256 << 10, 512 << 10, 1 << 20, 2 << 20};

// the core id that thread should be pinned
// use numactl --hardware command to check numa node info
static int kThreadIDs[16] = {16, 17, 18, 19, 20, 21, 22, 23, 0, 1, 2, 3, 4, 5, 6, 7};
// static int kThreadIDs[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

using namespace util;
using namespace std::placeholders;

class Benchmark {
public:

Benchmark(const std::string& path) {
    stats_ = std::vector<BenchData> (FLAGS_thread);
    for (int i = 0; i < FLAGS_thread; i++) {
        // for each thread, create a pmem file
        char* pmem_addr = nullptr;
        std::string filename = path+std::to_string(i);
        if ((pmem_addr = (char *)pmem_map_file(filename.c_str(), FLAGS_filesize, PMEM_FILE_CREATE, 0666, &mapped_len_, &is_pmem_)) == NULL) {
            perror("pmem_map_file");
            exit(1);
        }
        if (FLAGS_initfile) {
            printf("Initial file: %s. ", filename.c_str());
            pmem_memset(pmem_addr, 0, FLAGS_filesize, PMEM_F_MEM_NONTEMPORAL);
        }
        printf("pmem addr %2d: %lx\n", i, (uint64_t) pmem_addr);
        pmem_memset(pmem_addr, 0, 4096, PMEM_F_MEM_NONTEMPORAL);
        
        pmem_addrs_.push_back(pmem_addr);
    }
    
    auto pcm_ = PCM::getInstance();
    auto status = pcm_->program();
    if (status != PCM::Success)
    {
        std::cout << "Error opening PCM: " << status << std::endl;
        if (status == PCM::PMUBusy)
            pcm_->resetPMU();
        else
            exit(0);
    }
}

~Benchmark() {
    for (char* pmem_addr : pmem_addrs_)
        pmem_unmap(pmem_addr, mapped_len_);
}


// Return the throughput when using block_size to randomly write PMM
double FillRandomWB(uint64_t block_size, int thread_index, int thread_num) {
    printf("===== FillRandomWB, Thread: %d, Block Size: %10lu =====\r", thread_index, block_size);
    if (TheadCheck(thread_index) < 0) {
        exit(1);
    }
    // pin current thread to kThreadIDs[thread_index]
    // PinCore(kThreadIDs[thread_index]);

    TraceUniform trace(999 * thread_index + 123, 0, FLAGS_filesize - block_size);
    // choose which store function we should use
    void(*func)(char*,int);
    uint64_t off_interval = FLAGS_offset_interval < 0 ? block_size : FLAGS_offset_interval;
    uint64_t mask = PMMMask(off_interval);
    if (64 == block_size) {
        func = Store64_WB;
    } else if (128 == block_size) {
        func = Store128_WB;
    } else if (256 == block_size) {
        func = Store256_WB;
    } else if (512 == block_size) {
        func = Store512_WB;
    } else // if (
        // block_size % 1024 == 0 &&
        //block_size > 0) 
    {
        func = StoreNKB_WB;
    }

    // start fill random blocks
 
    int64_t k = 0;
    int64_t left_byte = FLAGS_filesize / thread_num;
    int N = block_size / 1024;
    char* pmem_addr = pmem_addrs_[thread_index];
    auto time_start = util::Env::Default()->NowMicros();
    for (; k < FLAGS_num  && left_byte > 0; k++) {
        uint64_t next = trace.Next();
        uint64_t offset = (next & mask);
        // INFO("Offset: %lx. next: %lu", offset, next);
        char* addr = pmem_addr + offset;
        if (FLAGS_pmdk)
            pmem_memset(addr, 0, block_size, PMEM_F_MEM_WB);
        else
            func(addr, N);
        left_byte -= block_size;
    }
    _mm_sfence();
    auto time_end = util::Env::Default()->NowMicros();
    double writes  = (block_size * k) / 1024.0 / 1024.0;
    double time   = (time_end - time_start) / 1000000.0;
    double throughput = writes / time;
    printf("Avg Random Write(WB), Write: %6.2f MB, Time: %4.3fs, T:%2d, %7lu byte - %7lu off interval, %8.1f MB/s\n", writes, time, thread_index, block_size, off_interval, throughput);
    return writes;
}

void LatencyProfiling() {

}


void XPBufferAssociateProfiler() {
    
    // PinCore(kThreadIDs[0]);
    char* pmemaddr = pmem_addrs_[0];

    uint64_t buffer_size = FLAGS_buffer << 10;
    uint64_t OFFSET = 14 << 10;
    int repeat = 10000;
    const uint64_t WRITE_LIMIT = 100 << 20;
    printf("----- XPBuffer Size: %lu ------\n", buffer_size);
    // write data with fixed OFFSET    
    uint64_t writen = 0;
    IPMWatcher watcher("xpbuffertype");
    while (repeat-- > 0 && writen < WRITE_LIMIT) { 
        uint64_t n = buffer_size / 64 / 4;
        uint64_t off = 0;
        while (n-- > 0) {
            char* addr = pmemaddr + off;
            Store64_NT(addr);
            off += OFFSET;
            writen += 64;
        }
        n = buffer_size / 64 / 4;
        off = 0;
        while (n-- > 0) {
            char* addr = pmemaddr + off;
            Store64_NT(addr + 64);
            off += OFFSET;
            writen += 64;
        }
        n = buffer_size / 64 / 4;
        off = 0;
        while (n-- > 0) {
            char* addr = pmemaddr + off;
            Store64_NT(addr + 128);
            off += OFFSET;
            writen += 64;
        }
        n = buffer_size / 64 / 4;
        off = 0;
        while (n-- > 0) {
            char* addr = pmemaddr + off;
            Store64_NT(addr + 192);
            off += OFFSET;
            writen += 64;
        }
    }            

}


void ReadBufferProfiler() {
    // PinCore(kThreadIDs[0]);
    char* pmemaddr = pmem_addrs_[0];
    // use N number of continous 256-byte interval, 
    // construct 64 byte random write within the intervals
    
    for (int64_t N = FLAGS_n_start; N <= FLAGS_n_end; N++) 
    {
        printf("\n======= Interval N is: %4ld (read %ld byte) =======\n", N, N * 256);
        IPMWatcher watcher("readBufferSize");
        std::vector<uint64_t> offsets;
        uint64_t interval_size = 256 * N;
        uint64_t unit_count = interval_size / 64;
        for (size_t i = 0; i < unit_count; ++i) {
            offsets.push_back(i * 64);
        }

        int repeat = FLAGS_repeat;
        int64_t READ = 100 << 20;
        int64_t read_byte = 0;
        char buffer[1 << 20];
        ClearCache();
        while (repeat-- > 0 && read_byte < READ)  {
            // randomize the offset
            std::random_shuffle(offsets.begin(), offsets.end());
            for (uint64_t off : offsets) {
                memcpy(buffer, pmemaddr + off, 64);
                // util::Load64_NT(pmemaddr + off);
                // _mm_lfence();
            }
            read_byte += interval_size;
            ClearCache();
        }
        printf("finished read: %lu byte\n", read_byte);

    }
}
 

void XPBufferTypeProfiler() {
    char* pmemaddr = pmem_addrs_[0];
    // use N number of continous 256-byte interval, 
    // construct 64 byte random write within the intervals
    for (int N = FLAGS_n_start; N <= FLAGS_n_end; N++) {
        printf("\n======= Interval N is: %4d (write %d byte) =======\n", N, N * 256);
        IPMWatcher watcher("xpbuffertype");
        std::vector<uint64_t> offsets;
        uint64_t interval_size = 256 * N;
        uint64_t unit_count = interval_size / 64;
        for (size_t i = 0; i < unit_count; ++i) {
            offsets.push_back(i * 64);
        }

        int repeat = FLAGS_repeat;
        int64_t left_byte = 100 << 20;
        while (repeat-- > 0 && left_byte > 0)  {
            // randomize the offset
            std::random_shuffle(offsets.begin(), offsets.end());

            for (uint64_t off : offsets) {
                util::Store64_NT(pmemaddr + off);
            }
            left_byte -= interval_size;
        }

    }
}
 
void XPBufferSizeProfiler() {
    char* pmemaddr = pmem_addrs_[0];
    // use N number of continous 256-byte interval
    // In the first round, update the first half 128 byte of the 256 byte
    // Then in the second round, update the last half 128 byte of the 256 byte
    // IPMWatcher watcher("wa");
    for (int N = FLAGS_n_start; N <= FLAGS_n_end; N++) {
        printf("\n======= Interval N is: %4d (write %d byte) =======\n", N, N * 256);
        // WriteAmplificationWatcher wa_watcher(watcher);
        util::PCMMetric pcm_monitor("wa");
        int repeat = FLAGS_repeat;
        int64_t left_byte = 100 << 20;
        IPMWatcher watcher("bench_matrix");
        auto start = watcher.Profiler();
        while (repeat-- > 0 && left_byte > 0) {
            for (int i = 0; i < N; ++i) {
                // update first 64 byte
                uint64_t off = (i * 256);
                char* dest = pmemaddr + off;
                util::Store64_NT(dest);
            };
            for (int i = 0; i < N; ++i) {
                // update second  64 byte
                uint64_t off = (i * 256);
                char* dest = pmemaddr + off + 64;
                util::Store64_NT(dest);
            };
            for (int i = 0; i < N; ++i) {
                // update third  64 byte
                uint64_t off = (i * 256);
                char* dest = pmemaddr + off + 128;
                util::Store64_NT(dest);
            };
            for (int i = 0; i < N; ++i) {
                // update last  64 byte
                uint64_t off = (i * 256);
                char* dest = pmemaddr + off + 192;
                util::Store64_NT(dest);
            };
            left_byte -= 256 * N;
        }
    }
}

// Return the throughput when using block_size to randomly write PMM
double FillSeqWB(uint64_t block_size, int thread_index, int thread_num) {
    printf("===== FillSeqWB, Thread index: %2d of %2d, Block Size: %10lu =====\r", thread_index, thread_num, block_size);
    if (TheadCheck(thread_index) < 0) {
        exit(1);
    }
    // pin current thread to kThreadIDs[thread_index]
    // PinCore(kThreadIDs[thread_index]);

    
    // choose which store function we should use
    void(*func)(char*,int);
    uint64_t off_interval = FLAGS_offset_interval < 0 ? block_size : FLAGS_offset_interval;
    uint64_t mask = PMMMask(off_interval);
    uint64_t start_off = (random() % (FLAGS_filesize / 2)) / off_interval * off_interval;
    TraceSeq trace(start_off, off_interval, 0, FLAGS_filesize - off_interval);
    if (64 == block_size) {
        func = Store64_WB;
    } else if (128 == block_size) {
        func = Store128_WB;
    } else if (256 == block_size) {
        func = Store256_WB;
    } else if (512 == block_size) {
        func = Store512_WB;
    } else if (
        block_size % 1024 == 0 &&
        block_size > 0) {
        func = StoreNKB_WB;
    }
    else {
        perror("Block size not support\n");
        exit(1);
    }

    // start fill random blocks
 
    int64_t k = 0;
    int64_t left_byte = FLAGS_filesize / thread_num;
    int N = block_size / 1024;
    char* pmem_addr = pmem_addrs_[thread_index];
    auto time_start = util::Env::Default()->NowMicros();
    for (; k < FLAGS_num  && left_byte > 0; k++) {
        uint64_t next = trace.Next();
        uint64_t offset = (next & mask);
        // INFO("Offset: %lx. next: %lu", offset, next);
        char* addr = pmem_addr + offset;
        if (FLAGS_pmdk)
            pmem_memset(addr, 0, block_size, PMEM_F_MEM_WB);
        else
            func(addr, N);
        left_byte -= block_size;
    }
    _mm_sfence();
    auto time_end = util::Env::Default()->NowMicros();
    double writes  = (block_size * k) / 1024.0 / 1024.0;
    double time   = (time_end - time_start) / 1000000.0;
    double throughput = writes / time;
    printf("Avg Seq Write(WB), Write: %6.2f MB, Time: %4.3fs, T:%2d, %7lu byte - %7lu off interval, %8.1f MB/s\n", writes, time, thread_index, block_size, off_interval, throughput);
    return writes;
}

// Return the throughput when using block_size to randomly write PMM
double FillSeqNT(uint64_t block_size, int thread_index, int thread_num) {
    printf("===== FillSeqNT, Thread index: %2d of %2d, Block Size: %10lu =====\r", thread_index, thread_num, block_size);
    if (TheadCheck(thread_index) < 0) {
        exit(1);
    }
    // pin current thread to kThreadIDs[thread_index]
    // PinCore(kThreadIDs[thread_index]);

    
    // choose which store function we should use
    void(*func)(char*,int);
    uint64_t off_interval = FLAGS_offset_interval < 0 ? block_size : FLAGS_offset_interval;
    uint64_t mask = PMMMask(off_interval);
    uint64_t start_off = (random() % (FLAGS_filesize / 2)) / off_interval * off_interval;
    TraceSeq trace(start_off, off_interval, 0, FLAGS_filesize - off_interval);
    if (64 == block_size) {
        func = Store64_NT;
    } else if (128 == block_size) {
        func = Store128_NT;
    } else if (256 == block_size) {
        func = Store256_NT;
    } else if (512 == block_size) {
        func = Store512_NT;
    } else if (
        block_size % 1024 == 0 &&
        block_size > 0) {
        func = StoreNKB_NT;
    }
    else {
        perror("Block size not support\n");
        exit(1);
    }

    // start fill random blocks
 
    int64_t k = 0;
    int64_t left_byte = FLAGS_filesize / thread_num;
    int N = block_size / 1024;
    char* pmem_addr = pmem_addrs_[thread_index];
    auto time_start = util::Env::Default()->NowMicros();
    for (; k < FLAGS_num  && left_byte > 0; k++) {
        uint64_t next = trace.Next();
        uint64_t offset = (next & mask);
        // INFO("Offset: %lx. next: %lu", offset, next);
        char* addr = pmem_addr + offset;
        if (FLAGS_pmdk)
            pmem_memset(addr, 0, block_size, PMEM_F_MEM_NONTEMPORAL);
        else
            func(addr, N);
        left_byte -= block_size;
    }
    _mm_sfence();
    auto time_end = util::Env::Default()->NowMicros();
    double writes  = (block_size * k) / 1024.0 / 1024.0;
    double time   = (time_end - time_start) / 1000000.0;
    double throughput = writes / time;
    printf("Avg Seq Write(NT), Write: %6.2f MB, Time: %4.3fs, T:%2d, %7lu byte - %7lu off interval, %8.1f MB/s\n", writes, time, thread_index, block_size, off_interval, throughput);
    return writes;
}

// Return the throughput when using block_size to randomly write data to PMM
double FillRandomNT(uint64_t block_size, int thread_index, int thread_num) {
    printf("===== FillRandomNT, Thread: %d, Block Size: %10lu =====\r", thread_index, block_size);
    if (TheadCheck(thread_index) < 0) {
        exit(1);
    }
    // pin current thread to kThreadIDs[thread_index]
    // PinCore(kThreadIDs[thread_index]);

    TraceUniform trace(999 * thread_index + 123, 0, FLAGS_filesize - block_size);
    // choose which store function we should use
    void(*func)(char*,int);
    uint64_t off_interval = FLAGS_offset_interval < 0 ? block_size : FLAGS_offset_interval;
    uint64_t mask = PMMMask(off_interval);
    if (64 == block_size) {
        func = Store64_NT;
    } else if (128 == block_size) {
        func = Store128_NT;
    } else if (256 == block_size) {
        func = Store256_NT;
    } else if (512 == block_size) {
        func = Store512_NT;
    } else if (
        block_size % 1024 == 0 &&
        block_size > 0) {
        func = StoreNKB_NT;
    }
    else {
        perror("Block size not support\n");
        exit(1);
    }

    // start fill random blocks
 
    int64_t k = 0;
    int64_t left_byte = FLAGS_filesize / thread_num;
    int N = block_size / 1024;
    char* pmem_addr = pmem_addrs_[thread_index];
    auto time_start = util::Env::Default()->NowMicros();
    for (; k < FLAGS_num  && left_byte > 0; k++) {
        uint64_t next = trace.Next();
        uint64_t offset = (next & mask);
        // INFO("Offset: %lx. next: %lu", offset, next);
        char* addr = pmem_addr + offset;
        if (FLAGS_pmdk)
            pmem_memset(addr, 0, block_size, PMEM_F_MEM_NONTEMPORAL);
        else 
            func(addr, N);
        left_byte -= block_size;
    }
    _mm_sfence();
    auto time_end = util::Env::Default()->NowMicros();
    double writes  = (block_size * k) / 1024.0 / 1024.0;
    double time   = (time_end - time_start) / 1000000.0;
    double throughput = writes / time;
    printf("Avg Random Write(NT), Write: %6.2f MB, Time: %4.3fs, T:%2d, %7lu byte - %7lu off interval, %8.1f MB/s\n", writes, time, thread_index, block_size, off_interval, throughput);
    return writes;
}


// Return the throughput when using block_size to randomly read data from PMM
double ReadRandomNT(uint64_t block_size, int thread_index, int thread_num) {
    printf("===== ReadRandomNT, Thread: %d, Block Size: %10lu =====\r", thread_index, block_size);
    if (TheadCheck(thread_index) < 0) {
        exit(1);
    }
    // pin current thread to kThreadIDs[thread_index]
    // PinCore(kThreadIDs[thread_index]);

    TraceUniform trace(999 * thread_index + 123, 0, FLAGS_filesize - block_size);
    // choose which store function we should use
    void(*func)(char*,int);
    uint64_t off_interval = FLAGS_offset_interval < 0 ? block_size : FLAGS_offset_interval;
    uint64_t mask = PMMMask(off_interval);
    if (64 == block_size) {
        func = Load64_NT;
    } else if (128 == block_size) {
        func = Load128_NT;
    } else if (256 == block_size) {
        func = Load256_NT;
    } else if (512 == block_size) {
        func = Load512_NT;
    } else if (
        block_size % 1024 == 0 &&
        block_size > 0) {
        func = LoadNKB_NT;
    }
    else {
        perror("Block size not support\n");
        exit(1);
    }

    // start read random blocks
    int64_t k = 0;
    int64_t left_byte = FLAGS_filesize / thread_num;
    int N = block_size / 1024;
    char* pmem_addr = pmem_addrs_[thread_index];
    auto time_start = util::Env::Default()->NowMicros();
    char buffer[4 << 20]; // initial a 4MB buffer
    for (; k < FLAGS_num  && left_byte > 0; k++) {
        uint64_t next = trace.Next();
        uint64_t offset = (next & mask);
        // INFO("Offset: %lx. next: %lu", offset, next);
        char* addr = pmem_addr + offset;
        if (FLAGS_load)
            func(addr, N);
        else
            memcpy(buffer, addr, block_size);
        left_byte -= block_size;
    }
    _mm_lfence();
    auto time_end = util::Env::Default()->NowMicros();
    double reads  = (block_size * k) / 1024.0 / 1024.0;
    double time   = (time_end - time_start) / 1000000.0;
    double throughput = reads / time;
    printf("Avg Random Read(%s), Read: %6.2f MB, Time: %4.3fs, T:%2d, %7lu byte - %7lu off interval, %8.1f MB/s\n", FLAGS_load ? "NT": "memcpy", reads, time, thread_index, block_size, off_interval, throughput);
    return reads;
}

// Return the throughput when using block_size to sequentially read data from PMM
double ReadSeqNT(uint64_t block_size, int thread_index, int thread_num) {
    printf("===== ReadSeqNT, Thread index: %2d of %2d, Block Size: %10lu =====\r", thread_index, thread_num, block_size);
    if (TheadCheck(thread_index) < 0) {
        exit(1);
    }
    // pin current thread to kThreadIDs[thread_index]
    // PinCore(kThreadIDs[thread_index]);

    
    // choose which store function we should use
    void(*func)(char*,int);
    uint64_t off_interval = FLAGS_offset_interval < 0 ? block_size : FLAGS_offset_interval;
    uint64_t start_off = (random() % (FLAGS_filesize / 2)) / off_interval * off_interval;
    TraceSeq trace(start_off, off_interval, 0, FLAGS_filesize - off_interval);

    uint64_t mask = PMMMask(off_interval);
    if (64 == block_size) {
        func = Load64_NT;
    } else if (128 == block_size) {
        func = Load128_NT;
    } else if (256 == block_size) {
        func = Load256_NT;
    } else if (512 == block_size) {
        func = Load512_NT;
    } else if (
        block_size % 1024 == 0 &&
        block_size > 0) {
        func = LoadNKB_NT;
    }
    else {
        perror("Block size not support\n");
        exit(1);
    }

    // start read sequential blocks
    int64_t k = 0;
    int64_t left_byte = FLAGS_filesize / thread_num;
    int N = block_size / 1024;
    char* pmem_addr = pmem_addrs_[thread_index];
    auto time_start = util::Env::Default()->NowMicros();
    char buffer[4 << 20]; // initial a 4 MB buffer
    for (; k < FLAGS_num  && left_byte > 0; k++) {
        uint64_t next = trace.Next();
        uint64_t offset = (next & mask);
        // INFO("Offset: %lx. next: %lu", offset, next);
        char* addr = pmem_addr + offset;
        if (FLAGS_load)
            func(addr, N);
        else
            memcpy(buffer, addr, block_size);
        left_byte -= block_size;
    }
    _mm_lfence();
    auto time_end = util::Env::Default()->NowMicros();
    double reads  = (block_size * k) / 1024.0 / 1024.0;
    double time   = (time_end - time_start) / 1000000.0;
    double throughput = reads / time;
    printf("Avg Seq Read(%s), Read: %6.2f MB, Time: %4.3fs, T:%2d, %7lu byte - %7lu off interval, %8.1f MB/s\n", FLAGS_load ? "NT": "memcpy", reads, time, thread_index, block_size, off_interval, throughput);
    return reads;
}

int TheadCheck(int thread_index) {
    if (thread_index >= 16) {
        perror("Too much thread\n");
        return -1;
    }
    return 0;
}
enum BenchType {T_BenchFillRandomNT, T_BenchFillRandomWB, T_BenchFillSeqNT, T_BenchFillSeqWB, T_BenchReadRandomNT, T_BenchReadSeqNT};

static enum BenchType DecodeStringToType(const std::string& type) {
    if (type == "fillrandomNT") {
        return T_BenchFillRandomNT;
    } 
    else if (type == "fillrandomWB") {
        return T_BenchFillRandomWB;
    }
    else if (type == "fillseqNT") {
        return T_BenchFillSeqNT;
    }
    else if (type == "fillseqWB") {
        return T_BenchFillSeqWB;
    }
    else if (type == "readrandomNT") {
        return T_BenchReadRandomNT;
    }
    else 
        return T_BenchReadSeqNT;
}

std::string DecodeTypeToString(BenchType type) {
    // printf("Decode type: %d\n", type);
    std::string res = "not support";
    switch (type)
    {
    case T_BenchFillRandomNT:
        res = "Fill Random NT";
        break;
    
    case T_BenchFillRandomWB:
        res = "Fill Random WB";
        break;

    case T_BenchFillSeqNT:
        res = "Fill Sequential NT";
        break;

    case T_BenchFillSeqWB:
        res = "Fill Sequential WB";
        break;

    case T_BenchReadRandomNT:
        res = "Read Random";
        break;
    
    case T_BenchReadSeqNT:
        res = "Read Sequential";
        break;

    default:
        break;
    }
    return res;
}


std::function<double(uint64_t, int, int)> DecodeBenchFunc(BenchType bench_type) {
    std::function<double(uint64_t, int, int)> bench_func;
    switch (bench_type)
    {
    case T_BenchFillRandomNT:
        bench_func = std::bind(&Benchmark::FillRandomNT, this, _1, _2, _3);
        break;
    
    case T_BenchFillSeqNT:
        bench_func = std::bind(&Benchmark::FillSeqNT, this, _1, _2, _3);
        break;
    
    case T_BenchFillRandomWB:
        bench_func = std::bind(&Benchmark::FillRandomWB, this, _1, _2, _3);
        break;

    case T_BenchFillSeqWB:
        bench_func = std::bind(&Benchmark::FillSeqWB, this, _1, _2, _3);
        break;

    case T_BenchReadRandomNT:
        bench_func = std::bind(&Benchmark::ReadRandomNT, this, _1, _2, _3);
        break;
    
    case T_BenchReadSeqNT:
        bench_func = std::bind(&Benchmark::ReadSeqNT, this, _1, _2, _3);
        break;

    default:
        bench_func = std::bind(&Benchmark::FillRandomNT, this, _1, _2, _3);
        break;
    }
    return bench_func;
}

void BenchSingle(BenchType bench_type) {
    printf("===== BenchSingle =====\r");
    auto bench_func = DecodeBenchFunc(bench_type);
    int thread_num = FLAGS_thread;
    std::vector<std::thread> workers(thread_num);
    std::vector<double> tmp(thread_num);
    auto time_start = Env::Default()->NowMicros();
    {
        PCMMetric pcm_monitor("single");
        // ------------------- benchmark function -------------------
        for (int t = 0; t < thread_num; t++) {
            workers[t] = std::thread([&, t]
            {
                // core function
                tmp[t] = bench_func(FLAGS_block_size, t, thread_num);
            });
        }
        std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
        {
            t.join();
        });
    }
    auto time_duration = Env::Default()->NowMicros() - time_start;
    double data_mb = std::accumulate(tmp.begin(), tmp.end(), 0);
    double final_throughput = data_mb / ( time_duration / 1000000.0);
    printf("\033[32m\nMode: single\nResult (%s. PMDK: %s)\033[0m\n", DecodeTypeToString(bench_type).c_str(), FLAGS_pmdk ? "true": "false");
    printf("\033[34m");
    printf("Block Size: %8d, Thread: %2d, Throughput: %6.2f \033[0m\n", FLAGS_block_size, FLAGS_thread, final_throughput);
    fflush(nullptr);
}

void BenchMatrix(BenchType bench_type) {
    auto bench_func = DecodeBenchFunc(bench_type);

    std::vector<std::vector<double> > result_matrix_read;
    std::vector<std::vector<double> > result_matrix_write;
    for (int thread_num = 1; thread_num <= FLAGS_thread; thread_num++) {
        std::vector<double> read_throughputs;
        std::vector<double> write_throughputs;
        // iterate all block size
        for (size_t i = 0; i < kBufferVector.size(); ++i) {
            auto block_size = kBufferVector[i];
            std::vector<std::thread> workers(thread_num);
            std::vector<double> tmp(thread_num);
            
            IPMWatcher watcher("bench_matrix");
            PCMMetric pcm_monitor("bench_matrix");
            // ------------------- benchmark function -------------------
            for (int t = 0; t < thread_num; t++) {
                workers[t] = std::thread([&, t]
                {
                    // core function
                    tmp[t] = bench_func(block_size, t, thread_num);
                });
            }
            std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
            {
                t.join();
            });
            _mm_sfence();
            watcher.Report();
            read_throughputs.push_back(watcher.app_read_);    
            write_throughputs.push_back(watcher.app_write_);
        }

        // print read result for all block size
        printf("\033[32m\nMode: matrix\nFinal Results (%s. %2d threads. PMDK: %s)\033[0m\n", DecodeTypeToString(bench_type).c_str(), thread_num, FLAGS_pmdk ? "true" : "false");
        printf("\033[34m\n");
        for (size_t i = 0; i < kBufferVector.size(); ++i) {
            printf("|-- %7lu B --", kBufferVector[i]);
        }
        printf("|\n");
        for (size_t i = 0; i < kBufferVector.size(); i++) {
            printf("|%10.1f MB/s", read_throughputs[i]);
        }
        printf("| Read\n");
        for (size_t i = 0; i < kBufferVector.size(); i++) {
            printf("|%10.1f MB/s", write_throughputs[i]);
        }
        printf("| Write\033[0m\n");
        fflush(nullptr);
        result_matrix_read.push_back(read_throughputs);
        result_matrix_write.push_back(write_throughputs);
    }

    // print results matrix
    printf("\033[32m\nMode: matrix\nResults Matrix (%s.)\033[0m\n", DecodeTypeToString(bench_type).c_str());
    // print read matrix
    printf("\033[34m Read Matrix:\n");
    for (size_t i = 0; i < kBufferVector.size(); ++i) {
        printf("|-- %7lu B --", kBufferVector[i]);
    }
    printf("|\n");
    for (size_t t = 0; t < result_matrix_read.size(); ++t) {
        for (size_t i = 0; i < kBufferVector.size(); i++) {
            printf("|%10.1f MB/s", result_matrix_read[t][i]);
        }
        printf("| %2lu\n", t + 1);
    }
    printf("|\033[0m\n");

    // print write matrix
    printf("\033[34m Write Matrix:\n");
    for (size_t i = 0; i < kBufferVector.size(); ++i) {
        printf("|-- %7lu B --", kBufferVector[i]);
    }
    printf("|\n");
    for (size_t t = 0; t < result_matrix_write.size(); ++t) {
        for (size_t i = 0; i < kBufferVector.size(); i++) {
            printf("|%10.1f MB/s", result_matrix_write[t][i]);
        }
        printf("| %2lu\n", t + 1);
    }
    printf("|\033[0m\n");

    fflush(nullptr);
}


void BenchReadAndWrite(uint64_t block_size) {
    auto read_func = DecodeBenchFunc(T_BenchReadRandomNT);
    auto write_func = DecodeBenchFunc(T_BenchFillRandomNT);

    int thread_num = FLAGS_thread;
    std::vector<std::thread> workers(thread_num);
    std::vector<double> read_throughput(thread_num, 0);
    std::vector<double> write_throughput(thread_num, 0);
    
    IPMWatcher watcher("all_thread");        
    PCMMetric pcm_monitor("all_thread");
    // ------------------- benchmark function -------------------
    for (int t = 0; t < thread_num; t++) {
        workers[t] = std::thread([&, t]
        {
            // core function
            if (t < thread_num / 4) {
                write_throughput[t] = write_func(block_size, t, 2);
            }
            else {
                read_throughput[t] = read_func(block_size, t, 2);
            }
            
        });
    }
    std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
    {
        t.join();
    });
    _mm_sfence();    
}

void BenchAllThread(BenchType bench_type, uint64_t block_size) {
    printf("BenchAllThread. bench_type: %d, %s\n", bench_type, DecodeTypeToString(bench_type).c_str());
    auto bench_func = DecodeBenchFunc(bench_type);
    std::vector<double> results_read;
    std::vector<double> results_write;
    for (int thread_num = 1; thread_num <= FLAGS_thread; thread_num++) {
        std::vector<std::thread> workers(thread_num);
        std::vector<double> tmp(thread_num);
        
        IPMWatcher watcher("all_thread");            
        PCMMetric pcm_monitor("all_thread");        
        // ------------------- benchmark function -------------------
        for (int t = 0; t < thread_num; t++) {
            workers[t] = std::thread([&, t]
            {
                // core function
                tmp[t] = bench_func(block_size, t, thread_num);
            });
        }
        std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
        {
            t.join();
        });
        _mm_sfence();
        watcher.Report();
        results_read.push_back(watcher.app_read_);    
        results_write.push_back(watcher.app_write_);
    }

    // print result for all thread giving block_size
    printf("\033[32m\nMode: row_thread\nFinal Results (%s. %2d threads, block size: %lu. PMDK: %s)\033[0m\n", DecodeTypeToString(bench_type).c_str(), FLAGS_thread, block_size, FLAGS_pmdk ? "true" : "false");
    printf("\033[34m");
    for (int i = 0; i < FLAGS_thread; ++i) {
        printf("|---- %3d ----", i+1);
    }
    printf("|\n");
    for (size_t i = 0; i < results_read.size(); i++) {
        printf("|%8.1f MB/s", results_read[i]);
    }
    printf("| Read\n");
    for (size_t i = 0; i < results_write.size(); i++) {
        printf("|%8.1f MB/s", results_write[i]);
    }
    printf("| Write\033[0m\n");
    fflush(nullptr);
}

void BenchAllBlockSize(BenchType bench_type, int thread_num) {
    auto bench_func = DecodeBenchFunc(bench_type);
    std::vector<double> throughputs_read;
    std::vector<double> throughputs_write;
    // iterate all block size
    for (size_t i = 0; i < kBufferVector.size(); ++i) {
        auto block_size = kBufferVector[i];
        std::vector<std::thread> workers(thread_num);
        std::vector<double> tmp(thread_num);
        IPMWatcher watcher("all_block");
        PCMMetric pcm_monitor("all_block");        
        // ------------------- benchmark function -------------------
        for (int t = 0; t < thread_num; t++) {
            workers[t] = std::thread([&, t]
            {
                // core function
                tmp[t] = bench_func(block_size, t, thread_num);
            });
        }
        std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
        {
            t.join();
        });
        _mm_sfence();
        watcher.Report();
        throughputs_read.push_back(watcher.app_read_);    
        throughputs_write.push_back(watcher.app_write_);
    }

    // print result for all block size
    printf("\033[32m\nMode: row_block\nFinal Results (%s. %2d threads. PMDK: %s)\033[0m\n", DecodeTypeToString(bench_type).c_str(), thread_num, FLAGS_pmdk ? "true" : "false");
    printf("\033[34m");
    for (size_t i = 0; i < kBufferVector.size(); ++i) {
        printf("|-- %7lu B --", kBufferVector[i]);
    }
    printf("|\n");
    for (size_t i = 0; i < kBufferVector.size(); i++) {
        printf("|%10.1f MB/s", throughputs_read[i]);
    }
    printf("| Read\n");
    for (size_t i = 0; i < kBufferVector.size(); i++) {
        printf("|%10.1f MB/s", throughputs_write[i]);
    }
    printf("| Write\033[0m\n");
    fflush(nullptr);
}

private:

    std::vector<uint64_t> GenerateOffsets(uint64_t num, bool is_seq, uint64_t MAX_BUF_LEN) {
        std::vector<uint64_t> offsets(num);
        if (!is_seq) {
            for (size_t k = 0; k < FLAGS_num; ++k) {
                offsets[k] = (random() % (FLAGS_filesize - MAX_BUF_LEN)) / MAX_BUF_LEN * MAX_BUF_LEN;
            }
        }
        else {
            uint64_t off = (random() % (FLAGS_filesize - MAX_BUF_LEN)) / MAX_BUF_LEN * MAX_BUF_LEN;
            for (size_t k = 0; k < FLAGS_num; ++k) {
                offsets[k] = off;
                off += MAX_BUF_LEN;
                if (off >= FLAGS_filesize - MAX_BUF_LEN) {
                    off = 0;
                }
            }
        }
        return offsets;
    }

    void PinCore(int i ) {
        // ------------------- pin current thread to core i -------------------
        // printf("Pin thread: %2d.\n", i);
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(i, &cpuset);
        pthread_t thread;
        thread = pthread_self();
        int rc = pthread_setaffinity_np(thread,
                                        sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            fprintf(stderr,"Error calling pthread_setaffinity_np: %d \n", rc);
        }
    }

    struct BenchData {
        std::vector<double> throughput;
    };
private:
    util::RandomGenerator gen;
    std::vector<char*> pmem_addrs_;
    size_t mapped_len_;
    int is_pmem_;
    std::vector<BenchData> stats_;
};


int main(int argc, char *argv[])
{
    // Set the default logger to file logger
    ParseCommandLineFlags(&argc, &argv, true);

    Benchmark bench(FLAGS_path);

    if (FLAGS_mode == "readbuffersize") {
        bench.ReadBufferProfiler();
    } else if (FLAGS_mode == "xpbufferAssociate") {
        bench.XPBufferAssociateProfiler();
    } else if (FLAGS_mode == "xpbuffersize")  {
        bench.XPBufferSizeProfiler();
    } else if (FLAGS_mode == "xpbuffertype" ) {
        bench.XPBufferTypeProfiler();
    } else if (FLAGS_mode == "row_block") {
        bench.BenchAllBlockSize(Benchmark::DecodeStringToType(FLAGS_type), FLAGS_thread);
    } else if (FLAGS_mode == "row_thread") {
        bench.BenchAllThread(Benchmark::DecodeStringToType(FLAGS_type), FLAGS_block_size);
    } else if (FLAGS_mode == "matrix" ) {
        bench.BenchMatrix(Benchmark::DecodeStringToType(FLAGS_type));
    } else if (FLAGS_mode == "single") {
        bench.BenchSingle(Benchmark::DecodeStringToType(FLAGS_type));
    } else if (FLAGS_mode == "rw") {
        bench.BenchReadAndWrite(FLAGS_block_size);
    }
    
    return 0;
}