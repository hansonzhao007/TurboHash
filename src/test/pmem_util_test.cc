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
#include "util/io_report.h"
#include "util/test_util.h"
#include "util/trace.h"
#include "util/pmm_util.h"


#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

using namespace util;

DEFINE_string(path, "/mnt/pmem0/test.txt", "default file path");
DEFINE_int64(filesize, 1ULL << 30, "default file size");



class Benchmark {
public:

Benchmark(const std::string& path) {
    if ((pmem_addr_ = (char *)pmem_map_file(path.c_str(), FLAGS_filesize, PMEM_FILE_CREATE, 0666, &mapped_len_, &is_pmem_)) == NULL) {
        perror("pmem_map_file");
        exit(1);
    }
    printf("pmem addr: %lx\n", (uint64_t) pmem_addr_);
    pmem_memset(pmem_addr_, 0, 1024, PMEM_F_MEM_NONTEMPORAL);
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
    pmem_unmap(pmem_addr_, mapped_len_);
}

void Test() {
    Env::Default()->PinCore(0);
    IPMWatcher watcher("profilerWPQ");
    int interval = 1024;
    auto metric_before = watcher.Profiler();
    PCMMetric pcm_monitor("single");
    uint64_t byte_read = FLAGS_filesize;
    uint64_t offset = 0;
    while (byte_read > 0) {
        Load256_NT(pmem_addr_ + offset);
        offset += 256;
        byte_read -=256;
    }
    auto metric_after = watcher.Profiler();
    IPMMetric metric(metric_before[0], metric_after[0]);
    printf("Read from IMC: %10lu, Read to DIMM: %10lu, Write to DIMM: %10lu, Write from IMC: %10lu \n",
        metric.GetByteReadFromIMC(),
        metric.GetByteReadToDIMM(),
        metric.GetByteWriteToDIMM(),
        metric.GetByteWriteFromIMC()
        );
}
private:
    char* pmem_addr_;
    size_t mapped_len_;
    int is_pmem_;

};

int main(int argc, char *argv[])
{
    // Set the default logger to file logger
    ParseCommandLineFlags(&argc, &argv, true);
    Benchmark bench(FLAGS_path);

    bench.Test();
    
}
