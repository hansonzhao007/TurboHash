#include <stdio.h>
#include <stdlib.h>
#include <libpmem.h>

#include "util/pmm_util.h"

int main()
{
    
    size_t file_size = 1LU << 30;
    std::string filename = "/mnt/pmem/pmem_test.data";
    char* pmem_addr = nullptr;
    size_t mapped_len;
    int   is_pmem;
    if ((pmem_addr = (char *)pmem_map_file(filename.c_str(), file_size, PMEM_FILE_CREATE, 0666, &mapped_len, &is_pmem)) == NULL) {
        perror("pmem_map_file");
        exit(1);
    }
    pmem_memset(pmem_addr, 0, file_size, PMEM_F_MEM_NONTEMPORAL);

    {
        util::IPMWatcher watcher("pmem");
        pmem_memset(pmem_addr, 0, file_size, PMEM_F_MEM_NONTEMPORAL);
    }
    return 0;
}