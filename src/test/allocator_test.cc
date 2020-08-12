#include <cstdlib>
#include "util/env.h"
#include "turbo/allocator.h"

using namespace util;
using namespace turbo;

int main() {
    char cmd[] = "cat /proc/meminfo | grep Huge | grep HugePages_Rsvd";
    {
        int count = 32768;
        MemBlock<CellMeta128> mem_block(0, count); // allocate 4 MB space for 32768 cells
        std::string res = Env::Default()->Execute(cmd);
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
    std::string res = Env::Default()->Execute(cmd);
    printf("%s", res.c_str());

    {
        int block_count = 10;
        MemAllocator<CellMeta128, 65536> allocater(block_count);
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
    return 0;
}