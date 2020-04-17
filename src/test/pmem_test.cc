#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libpmem.h>

#include <time.h>
#include "util/pmm_util.h"

#define PMEM_LEN ((size_t)8*1024*1024*1024)
#define RAND_STRING_16 ("OPTANEW optanew ")
#define RAND_STRING_END_16 ("XXXXXXX xxxxxxx\n")
#define RAND_STRING_32 ("OPTANEW optanew OPTANEW optanew ")
#define RAND_STRING_END_32 ("XXXXXXX xxxxxxx XXXXXXX xxxxxxx\n")
#define RAND_STRING_64 ("OPTANEW optanew OPTANEW optanew OPTANEW optanew OPTANEW optanew ")
#define RAND_STRING_END_64 ("XXXXXXX xxxxxxx XXXXXXX xxxxxxx XXXXXXX xxxxxxx XXXXXXX xxxxxxx\n")
#define STRING_LEN (64)
#define CACHELINE_SIZE (64)
#define WRITE_UNIT_SIZE (256)

#define WRITE_SIZE 256

using namespace util;
int main(int argc, char *argv[])
{
        Env::Default()->PinCore(0);
        int fd;
        char *pmemaddr;
        int is_pmem;
        size_t mapped_len;
        int write_size = WRITE_SIZE;
        struct timespec start_tv, end_tv;
        pmemaddr = (char*)pmem_map_file("/mnt/pmem0/zwh_mapped", PMEM_LEN, PMEM_FILE_CREATE, 0666, &mapped_len, &is_pmem);
        printf("Write size: %d\n", write_size);
        size_t round = PMEM_LEN / WRITE_SIZE;
        size_t string_num_per_round = WRITE_SIZE / STRING_LEN;
        pmem_drain();
        clock_gettime(CLOCK_REALTIME, &start_tv);

        util::IPMWatcher watcher("profilerWPQ");
        for (size_t i = 0; i < round; i++) {
                pmem_memset(pmemaddr+i*WRITE_SIZE, 0, 256, PMEM_F_MEM_NONTEMPORAL);
                // for (size_t j = 0; j < string_num_per_round - 1; j = j + 1) {
                //         pmem_memcpy(pmemaddr+i*WRITE_SIZE+j*STRING_LEN, RAND_STRING_64, STRING_LEN, PMEM_F_MEM_NOFLUSH|PMEM_F_MEM_NODRAIN);
                // }
                // pmem_memcpy(pmemaddr+i*WRITE_SIZE+(string_num_per_round-1)*STRING_LEN, RAND_STRING_END_64, STRING_LEN, PMEM_F_MEM_NOFLUSH|PMEM_F_MEM_NODRAIN);
                // pmem_flush(pmemaddr+i*WRITE_SIZE, WRITE_SIZE);
        }
        // pmem_drain();
        clock_gettime(CLOCK_REALTIME, &end_tv);
        long time_span = (end_tv.tv_sec - start_tv.tv_sec) * 1000000000 + (end_tv.tv_nsec - start_tv.tv_nsec);
        printf("start time: %ld, %ld\n", start_tv.tv_sec, start_tv.tv_nsec);
        printf("end time: %ld, %ld\n", end_tv.tv_sec, end_tv.tv_nsec);
        printf("total time spent: %ld ns\n", time_span);

        pmem_unmap(pmemaddr, PMEM_LEN);
        return 0;
}