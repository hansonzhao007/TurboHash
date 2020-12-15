#include <cstdlib>
#include "util/env.h"
#include "turbo/turbo_hash.h"
#include <thread>
#include <algorithm>
#include <unistd.h>


int main() {
    char cmd[] = "cat /proc/meminfo | grep Huge | grep HugePages_Rsvd";
    {
        int count = 32768;
        turbo::detail::MemBlock<turbo::detail::CellMeta128> mem_block(0, count); // allocate 4 MB space for 32768 cells
        std::string res = util::Env::Default()->Execute(cmd);
        printf("%s", res.c_str());

        int bucket_size = count / 32;
        for (int i = 0; i < count; i += bucket_size) {
            char* tmp = mem_block.Allocate(bucket_size);
            if (tmp == nullptr) {
                printf("Allocation fail. %d\n", i);
            }
        }
        printf("Reference should be 32. ref: %d\n", mem_block.Reference());

        char* tmp = mem_block.Allocate(1); 
        if (tmp != nullptr) {
            printf("MemBlock allocation should fail\n");
        }
        printf("MemBlock allocation test pass. Hugepage: %s\n", mem_block.IsHugePage() ? "true" : "false");
    }
    std::string res = util::Env::Default()->Execute(cmd);
    printf("%s", res.c_str());

    {
        int block_count = 10;
        turbo::detail::CellAllocator<turbo::detail::CellMeta128, 65536> allocater(block_count);
        for (int i = 0; i < block_count; i++) {
            auto tmp = allocater.AllocateNoSafe(65536);
            printf("mem block id: %d\n", tmp.first);
        }
        for (int i = 0; i < block_count; i++) {
            allocater.ReleaseNoSafe(i);
        }
        for (int i = 0; i < block_count; i++) {
            auto tmp = allocater.AllocateNoSafe(65536);
            printf("mem block id: %d\n", tmp.first);
        }

    }

    {
        int block_count = 10;
        turbo::detail::CellAllocator<turbo::detail::CellMeta128, 65536> allocater(block_count);
        int kLoops=512;
        int kNumThread = 8;
        std::vector<std::thread> workers2;
        auto start_time = util::Env::Default()->NowNanos();
        for (int i = 0; i < kNumThread; i++) {
            // each worker add 10000,
            workers2.push_back(std::thread([kLoops, &allocater, i]() 
            {   
                int loop = kLoops;
                while (loop--) {
                    auto tmp = allocater.AllocateSafe(128);
                    // ::usleep(1);
                    printf("thread %2d: mem block id: %d\n", i, tmp.first);
                }
            }));
        }
        std::for_each(workers2.begin(), workers2.end(), [](std::thread &t) 
        {
            t.join();
        });
        auto end_time = util::Env::Default()->NowNanos();
        printf("Speed: %f Mops/s.\n", (double)(kLoops * kNumThread)  / (end_time - start_time) * 1000.0);
    }
    return 0;
}