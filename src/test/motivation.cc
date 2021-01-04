#include <immintrin.h>
#include <cstdlib>
#include "turbo/hash_function.h"

#include "util/env.h"
#include "util/perf_util.h"
#include "util/pmm_util.h"

#include "ralloc.hpp"
#include "pptr.hpp"
#include "libpmem.h"

#include "gflags/gflags.h"

#define IS_PMEM 1

using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;
using namespace util;

DEFINE_int32(loop, 4, "");
DEFINE_string(filename, "motivation.csv", "");

const uint64_t MASK64 = (~(UINT64_C(63)));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"


struct PmemRoot {
    pptr<char> addr;
};

inline void // __attribute__((optimize("O0"),always_inline))
RndAccess(char* addr, uint64_t size_mask) {
    uint64_t off = turbo::wyhash64() & size_mask;
    int loop = FLAGS_loop;
    while (loop--) {
        uint64_t off = turbo::wyhash64() & size_mask;
        volatile char tmp = *(addr + off);
        off += 64;
    }
}

inline void  // __attribute__((optimize("O0"),always_inline))
ConAccess(char* addr, uint64_t size_mask) {
    uint64_t off = turbo::wyhash64() & size_mask;
    int loop = FLAGS_loop;
    while (loop--) {
        uint64_t off_tmp = turbo::wyhash64() & size_mask;
        volatile char tmp = *(addr + off);
        off += 64;
    }
}

inline void  // __attribute__((optimize("O0"),always_inline))
RndWrite(char* addr, uint64_t size_mask) {
    uint64_t off = turbo::wyhash64() & size_mask;
    int loop = FLAGS_loop;
    while (loop--) {
        uint64_t off = turbo::wyhash64() & size_mask;
        volatile char tmp = *(addr + off);
        memset(addr + off, 32, 8);
        off += 64;
    }
}

inline void  // __attribute__((optimize("O0"),always_inline))
ConWrite(char* addr, uint64_t size_mask) {
    uint64_t off = turbo::wyhash64() & size_mask;
    int loop = FLAGS_loop;
    while (loop--) {
        uint64_t off_tmp = turbo::wyhash64() & size_mask;
        volatile char tmp = *(addr + off);
        memset(addr + off, 32, 8);
        off += 64;
    }
}
#pragma GCC diagnostic pop

void AccessCacheLineSize() {
    const uint64_t repeat = 5000000;
    const uint64_t size = 4LU << 30;
    const uint64_t size_mask = (size - 1) & MASK64;
    uint64_t size_mask2 = (size - 1) & (~(64 * FLAGS_loop - 1));

    char* addr = nullptr;
    #ifdef IS_PMEM
    auto res = RP_init("motivation", size * 2);
    if (res) {
        printf("Rmapping, prepare to recover\n");
        RP_get_root<PmemRoot>(0);
        int recover_res = RP_recover();
        if (recover_res == 1) {
            printf("Dirty open, recover\n");
        } else {
            printf("Clean restart.\n");
        }
        PmemRoot* root = RP_get_root<PmemRoot>(0);
        addr = root->addr;
    } else {
        printf("Clean create\n");
        void* buf = RP_malloc(sizeof(PmemRoot));
        PmemRoot* root = static_cast<PmemRoot*>(buf);
        root->addr = (char*) RP_malloc(size);
        addr = root->addr;
        memset(addr, 0, size);
        FLUSH(root);
        FLUSHFENCE;
        RP_set_root(buf, 0);
    }
    #else
    addr = (char*)aligned_alloc(64, size);
    memset(addr, 0, size);
    #endif

    auto file = fopen(FLAGS_filename.c_str(), "a");
    fprintf(file, "%d, ", FLAGS_loop);
    
    {
        #ifdef IS_PMEM
        IPMWatcher watcher("rnd_read");
        #endif
        debug_perf_switch();
        auto time_start = Env::Default()->NowNanos();
        for (uint64_t i = 0; i < repeat; i++) {
            RndAccess(addr, size_mask);
        }
        auto time_end   = Env::Default()->NowNanos();
        double duration = time_end - time_start;
        fprintf(file, "%f, ", duration / repeat);
    }
    
    {
        #ifdef IS_PMEM
        IPMWatcher watcher("seq_read");
        #endif
        debug_perf_switch();
        auto time_start = Env::Default()->NowNanos();
        for (uint64_t i = 0; i < repeat; i++) {
            ConAccess(addr, size_mask2);
        }
        auto time_end   = Env::Default()->NowNanos();
        double duration = time_end - time_start;
        fprintf(file, "%f, ", duration / repeat);
    }

    {
        #ifdef IS_PMEM
        IPMWatcher watcher("rnd_write");
        #endif
        debug_perf_switch();
        auto time_start = Env::Default()->NowNanos();
        for (uint64_t i = 0; i < repeat; i++) {
            RndWrite(addr, size_mask);
        }
        auto time_end   = Env::Default()->NowNanos();
        double duration = time_end - time_start;
        fprintf(file, "%f, ", duration / repeat);
    }

    {
        #ifdef IS_PMEM
        IPMWatcher watcher("seq_write");
        #endif
        debug_perf_switch();
        auto time_start = Env::Default()->NowNanos();
        for (uint64_t i = 0; i < repeat; i++) {
            ConWrite(addr, size_mask2);
        }
        auto time_end   = Env::Default()->NowNanos();
        double duration = time_end - time_start;
        fprintf(file, "%f, ", duration / repeat);
    }
    debug_perf_stop();
    fflush(file);
    fclose(file);

    #ifdef IS_PMEM
    RP_close();
    #endif
}

int main(int argc, char *argv[]) {
    ParseCommandLineFlags(&argc, &argv, true);
    debug_perf_ppid();
    // Contiguous access is more efficient
    AccessCacheLineSize();
    return 0;
}