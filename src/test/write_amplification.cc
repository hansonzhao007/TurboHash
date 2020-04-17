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
#include <numeric>
#include <linux/membarrier.h>
#include <immintrin.h>

#include "cpucounters.h"

#include "util/env.h"
#include "util/io_report.h"
#include "util/test_util.h"
#include "util/trace.h"
#include "util/pmm_util.h"

#include "cpucounters.h"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

DEFINE_string(path, "/mnt/pmem0/test.txt", "default file path");
DEFINE_int64(filesize, 2ULL << 30, "default file size");
DEFINE_int32(n, 128, "how many 256 unit should be assigned");
DEFINE_int32(repeat, 1000, "repeat write");
DEFINE_int32(start, 16, "");

using namespace util;
int main(int argc, char *argv[]) {
    // Set the default logger to file logger
    ParseCommandLineFlags(&argc, &argv, true);

    Env::Default()->PinCore(0);

    char *pmemaddr;
    size_t mapped_len;
    int is_pmem;

    if ((pmemaddr = (char *)pmem_map_file(FLAGS_path.c_str(), FLAGS_filesize, PMEM_FILE_CREATE, 0666, &mapped_len, &is_pmem)) == NULL) {
        perror("pmem_map_file");
        exit(1);
    }

    printf("is pmem: %d. mapped size: %lu\n", is_pmem, mapped_len);
    printf("pmem address is: 0x%lx\n", (uint64_t)pmemaddr);
    pmem_memset(pmemaddr, 0, 256, PMEM_F_MEM_NONTEMPORAL);

    IPMWatcher watcher("wa");
    uint64_t kInterval = 256;
    __m512i zmm = _mm512_set1_epi8((char)(31));

    // {
    //     // test IPMWatcher overhead
    //     MFence();
    //     WriteAmplificationWatcher wa_watcher(watcher);
    //     return 0;
    // }
    {
        for (int N = FLAGS_start; N <= FLAGS_n; ++N) {
            printf("\n======= Interval N is: %4d (write %lu byte) =======\n", N, N * kInterval);
            WriteAmplificationWatcher wa_watcher(watcher);
            util::PCMMetric pcm_monitor("wa");
            int repeat = FLAGS_repeat;
            int64_t left_byte = 100 << 20;
            IPMWatcher watcher("bench_matrix");
            auto start = watcher.Profiler();
            auto time_start = Env::Default()->NowMicros();
            while (repeat-- > 0 && left_byte > 0) {
                for (int i = 0; i < N; ++i) {
                    // update first half 128 byte
                    uint64_t off = (i * kInterval);
                    char* dest = pmemaddr + off;
                    util::Store128_NT(dest);
                };

                for (int i = 0; i < N; ++i) {
                    // update last half 128 byte
                    uint64_t off = (i * kInterval);
                    char* dest = pmemaddr + off + 128;
                    util::Store128_NT(dest);
                };
                left_byte -= 256 * N;
            }
            auto time_end = Env::Default()->NowMicros();
            auto end = watcher.Profiler();
            auto duration = time_end - time_start;
            IPMMetric metric(start[0], end[0]);
            printf("\033[34m------- DIMM Read: %8.1f MB/s. DIMM Write: %8.1f MB/s --------\033[0m\n", 
                metric.GetByteReadToDIMM() / 1024.0/1024.0/ (duration / 1000000.0),
                metric.GetByteWriteToDIMM() / 1024.0/1024.0/ (duration / 1000000.0));
        }
    }
    
    return 0;
}